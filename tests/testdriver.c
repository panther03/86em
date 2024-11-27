#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <json-c/arraylist.h>
#include <json-c/json_object.h>
#include <json-c/json_tokener.h>
#include <json-c/json_util.h>
#include <json-c/linkhash.h>
#include <zlib.h>

#include "vm.h"

#define CHUNK 32768
#define TESTSUITE_PASS 0
#define TESTSUITE_FAIL -1
#define TESTSUITE_PARSE_FAIL -2
#define TESTSUITE_PARSE_CONTINUE 1

#define FAIL_RED "\e[0;31mfail\e[0m"
#define PASS_GRN "\e[0;32mpass\e[0m"

typedef int (*testsuite_run_fn_t)(struct json_object *);

///////////////////
// Global state //
/////////////////
vm_t *vm = NULL;
int stopearly = 0;
int testcaselimit = -1;
int testcaseind = 0;
int filter = 0;
size_t json_total_read = 0;

// Constants
uint8_t REG_LUT[26 * 26] = {0};

#define REG_HASH(x) (((uint)x[0]-0x61) * 26 + ((uint)x[1]-0x61))

static inline void init_reg_lut() {
    // Add 1 so that 0 is an invalid default state
    REG_LUT[REG_HASH("ax")] = 1 + (uint8_t)offsetof(x86_cpu_t, a);
    REG_LUT[REG_HASH("bx")] = 1 + (uint8_t)offsetof(x86_cpu_t, b);
    REG_LUT[REG_HASH("cx")] = 1 + (uint8_t)offsetof(x86_cpu_t, c);
    REG_LUT[REG_HASH("dx")] = 1 + (uint8_t)offsetof(x86_cpu_t, d);
    REG_LUT[REG_HASH("cs")] = 1 + (uint8_t)offsetof(x86_cpu_t, cs);
    REG_LUT[REG_HASH("ss")] = 1 + (uint8_t)offsetof(x86_cpu_t, ss);
    REG_LUT[REG_HASH("ds")] = 1 + (uint8_t)offsetof(x86_cpu_t, ds);
    REG_LUT[REG_HASH("es")] = 1 + (uint8_t)offsetof(x86_cpu_t, es);
    REG_LUT[REG_HASH("sp")] = 1 + (uint8_t)offsetof(x86_cpu_t, sp);
    REG_LUT[REG_HASH("bp")] = 1 + (uint8_t)offsetof(x86_cpu_t, bp);
    REG_LUT[REG_HASH("si")] = 1 + (uint8_t)offsetof(x86_cpu_t, si);
    REG_LUT[REG_HASH("di")] = 1 + (uint8_t)offsetof(x86_cpu_t, di);
    REG_LUT[REG_HASH("ip")] = 1 + (uint8_t)offsetof(x86_cpu_t, ip);
    REG_LUT[REG_HASH("fl")] = 1 + (uint8_t)offsetof(x86_cpu_t, flags);
}

//////////////////////////
// Testsuite functions //
////////////////////////

#define TESTCASE_ASSERT assert

static inline const char* json_get_string(const struct json_object *testcase, const char *key) {
    struct json_object *str_obj = json_object_object_get(testcase, key);
    TESTCASE_ASSERT(str_obj != NULL);
    const char *str = json_object_to_json_string(str_obj);
    TESTCASE_ASSERT(str != NULL);
    return str;
}

static inline const array_list* json_get_array(const struct json_object *testcase, const char *key) {
    struct json_object *array_obj = json_object_object_get(testcase, key);
    TESTCASE_ASSERT(array_obj != NULL);
    const array_list *array = json_object_get_array(array_obj);
    TESTCASE_ASSERT(array != NULL);
    return array;
}

int run_testcase(struct json_object *testcase) {
    const char *name = json_get_string(testcase, "name");
    if (!filter) printf("%s: ", name);

    const struct json_object *initial = json_object_object_get(testcase, "initial");
    TESTCASE_ASSERT(initial != NULL);

    const struct json_object *regs = json_object_object_get(initial, "regs");
    json_object_object_foreach(regs, regs_name, regs_val_obj) {
        uint reg_hash = REG_HASH(regs_name);
        TESTCASE_ASSERT(reg_hash < sizeof(REG_LUT));
        int reg_ofs = REG_LUT[reg_hash];
        TESTCASE_ASSERT(reg_ofs > 0);
        int reg_val = json_object_get_int(regs_val_obj);
        // TODO endianness?
        ((uint16_t*)&vm->cpu)[reg_ofs >> 1] = reg_val; 
    }

    const array_list *ram_init = json_get_array(initial, "ram");
    for (size_t i = 0; i < ram_init->length; i++) {
        const struct json_object *ram_entry = (struct json_object*)ram_init->array[i];
        const array_list *ram_entry_arr  = json_object_get_array(ram_entry);
        TESTCASE_ASSERT(ram_entry_arr->length == 2);
        int addr = json_object_get_int(ram_entry_arr->array[0]); 
        uint8_t val = json_object_get_int(ram_entry_arr->array[1]);
        store_u8(addr, val);
    }

    vm_run(vm, 1);

    const struct json_object *final = json_object_object_get(testcase, "final");
    TESTCASE_ASSERT(final != NULL);
    regs = json_object_object_get(final, "regs");
    int ret = 0;
    json_object_object_foreach(regs, regs_name_f, regs_val_obj_f) {
        uint reg_hash = REG_HASH(regs_name_f);
        TESTCASE_ASSERT(reg_hash < sizeof(REG_LUT));
        int reg_ofs = REG_LUT[reg_hash];
        TESTCASE_ASSERT(reg_ofs > 0);
        int expected = json_object_get_int(regs_val_obj_f);
        int actual = ((uint16_t*)&vm->cpu)[reg_ofs >> 1];
        // TODO endianness?
        if (expected != actual) {
            fprintf(stdout,"\n\t%s expected %04x got %04x", regs_name_f, expected, actual);
            ret = -1;
        }
    }

    const array_list *ram_final = json_get_array(final, "ram");
    for (size_t i = 0; i < ram_final->length; i++) {
        const struct json_object *ram_entry = (struct json_object*)ram_final->array[i];
        const array_list *ram_entry_arr  = json_object_get_array(ram_entry);
        TESTCASE_ASSERT(ram_entry_arr->length == 2);
        int addr = json_object_get_int(ram_entry_arr->array[0]); 
        uint8_t expected = json_object_get_int(ram_entry_arr->array[1]);
        uint8_t actual = load_u8(addr);
        if (expected != actual) {
            fprintf(stdout,"\n\tram[%x] expected %04x got %04x", addr, expected, actual);
            ret = -1; 
        }
    }

    
    if (ret) {
        printf("\n  ... "FAIL_RED" (bytes:");
        const array_list *bytes = json_get_array(testcase, "bytes");
        for (size_t i = 0; i < bytes->length; i++) {
            const struct json_object *byte_obj = (struct json_object*)bytes->array[i];
            const uint8_t byte = json_object_get_int(byte_obj);
            printf(" %02x", byte);
        }
        printf(")\n");
    } else if (!filter) {
        printf(PASS_GRN"\n");
    }

    return ret;
}

int run_testsuite(struct json_object *obj) {
    assert(obj != NULL);

    array_list *testcases = json_object_get_array(obj);
    if (testcases == NULL) {
        fprintf(stderr, "Testsuite format error\n");
        return TESTSUITE_PARSE_FAIL;
    }
    if (testcaseind >= testcases->length) {
        fprintf(stderr, "Testcase index out of bounds: %d\n", testcaseind);
        return TESTSUITE_FAIL;
    }
    int testcaselimit_adj = testcaselimit + testcaseind;
    size_t limit = (testcaselimit > 0 && (size_t)testcaselimit_adj < testcases->length) ? ((size_t)testcaselimit_adj) : testcases->length;
    int f_ret = TESTSUITE_PASS;
    int pass_count = 0;
    for (size_t i = testcaseind; i < limit; i++) {
        if (!filter) printf("[%ld] ", i);
        int t_ret = run_testcase((json_object *)testcases->array[i]);
        if (t_ret != 0) {
            f_ret = TESTSUITE_FAIL;
            if (filter) printf("[%ld]", i);
            if (stopearly) return f_ret;
        }
        pass_count += (1 + t_ret);
    }
    printf("%d/%ld passed \n", pass_count, limit-testcaseind);
    return f_ret;
}

/*
Parse JSON until a valid JSON object is recognized in the buffer. Data
afterwards is therefore discarded. Return TESTSUITE_PARSE_CONTINUE if it is
expecting more data, TESTSUITE_PARSE_FAIL if the parse failed, otherwise the
return code of the callback procedure.
*/
int parse_json_chunk(json_tokener *tok, const char *buf, size_t bufsz,
                     testsuite_run_fn_t callback) {
    json_total_read += bufsz;
    size_t start_pos = 0;
    struct json_object *obj;
    while (start_pos != bufsz) {
        obj = json_tokener_parse_ex(tok, &buf[start_pos], bufsz - start_pos);
        enum json_tokener_error jerr = json_tokener_get_error(tok);
        size_t parse_end = json_tokener_get_parse_end(tok);
        if (obj == NULL && jerr != json_tokener_continue) {
            const char *aterr = (start_pos + parse_end < (int)sizeof(buf))
                                    ? &buf[start_pos + parse_end]
                                    : "";
            fflush(stdout);
            size_t fail_offset =
                json_total_read - bufsz + start_pos + parse_end;
            fprintf(stderr, "Parse failed at offset %lu: %s %c\n",
                    (unsigned long)fail_offset, json_tokener_error_desc(jerr),
                    aterr[0]);
            return TESTSUITE_PARSE_FAIL;
        }
        if (obj != NULL) {
            int cb_ret = callback(obj);
            json_object_put(obj);
            assert(cb_ret < TESTSUITE_PARSE_CONTINUE);
            return cb_ret;
        }
        start_pos += json_tokener_get_parse_end(tok);
        assert(start_pos <= bufsz);
    }
    return TESTSUITE_PARSE_CONTINUE;
}

int parse_testsuite(FILE *testsuite_f, json_tokener *tok,
                    testsuite_run_fn_t callback) {
    int f_ret = TESTSUITE_PASS; //  return of this function
    int z_ret = Z_OK;           //  return of zlib library functions
    int j_ret = TESTSUITE_PASS; //  return of json parse function
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    // Allocate inflate state
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    z_ret = inflateInit2(&strm, 16 + MAX_WBITS);
    if (z_ret != Z_OK) {
        fprintf(stderr, "Could not initialize zlib inflate.\n");
        return TESTSUITE_PARSE_FAIL;
    }

    // Reset json read counter for error printing
    json_total_read = 0;

    // Decompress until inflate stream ends or end of file
    do {
        strm.avail_in = fread(in, 1, CHUNK, testsuite_f);
        if (ferror(testsuite_f)) {
            fprintf(stderr, "Could not read from testsuite file\n");
            f_ret = TESTSUITE_PARSE_FAIL;
            goto CLEANUP;
        }
        // If we're still here and there's no more data in the zip file,
        // we failed to parse it.
        if (strm.avail_in == 0)
            break;

        strm.next_in = in;

        // run deflate() on input until output buffer not full, finish
        // compression if all of source has been read in
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            z_ret = inflate(&strm, Z_NO_FLUSH);
            assert(z_ret != Z_STREAM_ERROR);    // state not clobbered
            switch (z_ret) {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                fprintf(stderr, "Inflate error: %d", j_ret);
                f_ret = TESTSUITE_PARSE_FAIL;
                goto CLEANUP;
            }
            have = CHUNK - strm.avail_out;
            j_ret = parse_json_chunk(tok, (const char *)out, have, callback);
            if (j_ret <= 0) {
                f_ret = j_ret;
                goto CLEANUP;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0); // all input will be used
    } while (z_ret != Z_STREAM_END);

    fprintf(stderr, "EOF reached in test file before JSON parsed\n");
    f_ret = TESTSUITE_PARSE_FAIL;

CLEANUP:

    // finish inflate
    (void)inflateEnd(&strm);

    // reset tokener
    json_tokener_reset(tok);

    return f_ret;
}

int main(int argc, char **argv) {
    opterr = 0;
    int c;
    int vmdbg = 0;

    while ((c = getopt(argc, argv, "fsdl:i:")) != -1) {
        switch (c) {
        case 's':
            stopearly = 1;
            break;
        case 'f':
            filter = 1;
            break;
        case 'i':
            testcaseind = atoi(optarg);
            if (testcaseind <= 0) {
                fprintf(stderr, "Invalid index provided: %d\n", testcaseind);
                return -1;
            }
            break;
        case 'l':
            testcaselimit = atoi(optarg);
            if (testcaselimit <= 0) {
                fprintf(stderr, "Invalid limit provided: %d\n", testcaselimit);
                return -1;
            }
            break;
        case 'd':
            vmdbg = 1;
            break;
        default: {
            fprintf(stderr, "Unexpected option %c\n", c);
            return -1;
        }
        }
    }

    if (argc - optind < 1) {
        fprintf(stderr, "Expected path(s) to gzipped JSON test case.\n");
        return -1;
    }

    vm = vm_init();
    init_mem_blank();
    init_reg_lut();

    if (vmdbg) {
        vm->opts.enable_trace = true;
    }

    /* allocate JSON tokenizer */
    json_tokener *tok = json_tokener_new_ex(JSON_TOKENER_DEFAULT_DEPTH);
    if (!tok) {
        fprintf(stderr, "Could not allocate JSON tokenizer.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = optind; i < argc; i++) {
        const char *testsuite = argv[i];
        FILE *testsuite_f = fopen(testsuite, "r");
        if (!testsuite_f) {
            printf("Could not read test suite: %s\n", testsuite);
            exit(EXIT_FAILURE);
        }
        int ret = parse_testsuite(testsuite_f, tok, run_testsuite);
        fclose(testsuite_f);
        if (ret != 0) {
            printf("%s:  "FAIL_RED"(%d)\n", testsuite, ret);
            if (stopearly) {
                json_tokener_free(tok);
                return -1;
            }
        } else {
            printf("%s: "PASS_GRN"\n", testsuite);
        }
    }

    json_tokener_free(tok);
    return 0;
}
