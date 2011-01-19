#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <stdlib.h>                    /* for malloc() */
#include <string.h>                    /* for memcpy() */
#include <stdio.h>                     /* for FILE, fopen(), etc. */
#include <assert.h>
#include <pthread.h>
#include <papi.h>

/* Internals */
#include "ptl_internal_papi.h"
#include "ptl_internal_commpad.h"      /* for proc_number */

#define MAX_PAPI_EVENTS 100

static unsigned int numCounters = 0;
static int papi_events[MAX_PAPI_EVENTS];
static FILE *papi_out = NULL;
long long **papi_ctrs = NULL;
long long **papi_measurements = NULL;
long long ***papi_sums = NULL;

typedef struct _event_disc {
    long unsigned int id;
    char *papi;
    char *shortnick;
    char *nick;
} event_disc_t;

static const event_disc_t event_lookup[] = {
/*{{{*/
    {PAPI_TOT_INS, "TOT_INS", "INS", "INSTRUCTIONS"},
    {PAPI_TOT_IIS, "TOT_IIS", "ISS", "ISSUED"},
    {PAPI_TOT_CYC, "TOT_CYC", "CYC", "CYCLES"},
    /* Branches */
    {PAPI_BR_INS, "BR_INS", "BRAN", "BRANCHES"},
    {PAPI_BR_CN, "BR_CN", "CBRN", "COND_BRANCHES"},
    {PAPI_BR_MSP, "BR_MSP", "MISP", "MISPREDICTIONS"},
    {PAPI_BR_TKN, "BR_TKN", "TBRN", "BRANCHES_TAKEN"},
    {PAPI_BR_NTK, "BR_NTK", "NBRN", "BRANCHES_NOT_TAKEN"},
    {PAPI_BR_PRC, "BR_PRC", "CPRD", "CORRECT_PREDICTIONS"},
    {PAPI_BR_UCN, "BR_UCN", "UBRN", "UNCOND_BRANCHES"},
    {PAPI_BRU_IDL, "BRU_IDL", "BIDL", "IDLE_BRANCH_CYCLES"},
    {PAPI_BTAC_M, "BTAC_M", "BTMS", "BRANCH_TCACHE_MISSES"},
    /* Cache */
    {PAPI_CA_CLN, "CA_CLN", "C_CLN", "EXCL_CLEAN_CACHE"},
    {PAPI_CA_INV, "CA_INV", "C_INV", "CACHE_INVALIDATION"},
    {PAPI_CA_ITV, "CA_ITV", "C_INT", "CACHE_INTERVENTION"},
    {PAPI_CA_SHR, "CA_SHR", "C_SHR", "EXCL_SHRD_CACHE_ACC"},
    {PAPI_CA_SNP, "CA_SNP", "SNOOP", "SNOOPS"},
    /* Floating Point */
    {PAPI_FAD_INS, "FAD_INS", "FADD", "FLOAT_ADDS"},
    {PAPI_FDV_INS, "FDV_INS", "FDIV", "FLOAT_DIVS"},
    {PAPI_FMA_INS, "FMA_INS", "FMAD", "FLOAT_MADDS"},
    {PAPI_FML_INS, "FML_INS", "FMUL", "FLOAT_MULTS"},
    {PAPI_FNV_INS, "FNV_INS", "FINV", "FLOAT_INVS"},
    {PAPI_FP_INS, "FP_INS", "FL", "FLOATS"},
    {PAPI_FP_OPS, "FP_OPS", "FLOP", "FLOAT_OP_EST"},
    {PAPI_FP_STAL, "FP_STAL", "FSTL", "FLOAT_STALLS"},
    {PAPI_FPU_IDL, "FPU_IDL", "FIDL", "FLOAT_IDLE_CYCS"},
    {PAPI_FSQ_INS, "FSQ_INS", "FSQT", "FLOAT_SQRTS"},
    /* Integer */
    {PAPI_FXU_IDL, "FXU_IDL", "IIDL", "INT_IDLE"},
    {PAPI_INT_INS, "INT_INS", "INTS", "INTS"},
    /* Misc */
    {PAPI_STL_CCY, "STL_CCY", "NCMP", "NO_COMPLETIONS"},
    {PAPI_STL_ICY, "STL_ICY", "NISS", "NO_ISSUES"},
    {PAPI_SYC_INS, "SYC_INS", "SYNC", "SYNCS"},
    {PAPI_FUL_CCY, "FUL_CCY", "MCYC", "MAXED_CYCS"},
    {PAPI_FUL_ICY, "FUL_ICY", "MICY", "ISSUE_MAXED_CYCS"},
    {PAPI_HW_INT, "HW_INT", "INTR", "INTERRUPTS"},
    {PAPI_VEC_INS, "VEC_INS", "VECS", "VECTORS"},
    /* Memory */
    {PAPI_LD_INS, "LD_INS", "LOAD", "LOADS"},
    {PAPI_LST_INS, "LST_INS", "MEM", "MEM_ACCESSES"},
    {PAPI_LSU_IDL, "LSU_IDL", "MIDL", "IDLE_MEM"},
    {PAPI_MEM_RCY, "MEM_RCY", "RSTL", "READ_STALLS"},
    {PAPI_MEM_SCY, "MEM_SCY", "MSTL", "MEMORY_STALLS"},
    {PAPI_MEM_WCY, "MEM_WCY", "WSTL", "WRITE_STALLS"},
    {PAPI_PRF_DM, "PRF_DM", "DPFM", "DATA_PREFETCH_MISSES"},
    {PAPI_RES_STL, "RES_STL", "STAL", "STALLS"},
    {PAPI_SR_INS, "SR_INS", "STOR", "STORES"},
    {PAPI_CSR_FAL, "CSR_FAL", "CSF", "CONDSTOR_FAILS"},
    {PAPI_CSR_SUC, "CSR_SUC", "CSS", "CONDSTOR_SUCCS"},
    {PAPI_CSR_TOT, "CSR_TOT", "CS", "CONDSTORS"},
    /* L1 Cache */
    {PAPI_L1_DCA, "L1_DCA", "L1DA", "L1_DATA_ACCESSES"},
    {PAPI_L1_DCH, "L1_DCH", "L1DH", "L1_DATA_HITS"},
    {PAPI_L1_DCM, "L1_DCM", "L1DM", "L1_DATA_MISSES"},
    {PAPI_L1_DCR, "L1_DCR", "L1DR", "L1_DATA_READS"},
    {PAPI_L1_DCW, "L1_DCW", "L1DW", "L1_DATA_WRITES"},
    {PAPI_L1_ICA, "L1_ICA", "L1IA", "L1_INST_ACCESSES"},
    {PAPI_L1_ICH, "L1_ICH", "L1IH", "L1_INST_HITS"},
    {PAPI_L1_ICM, "L1_ICM", "L1IM", "L1_INST_MISSES"},
    {PAPI_L1_ICR, "L1_ICR", "L1IR", "L1_INST_READS"},
    {PAPI_L1_ICW, "L1_ICW", "L1IW", "L1_INST_WRITES"},
    {PAPI_L1_LDM, "L1_LDM", "L1LM", "L1_LOAD_MISSES"},
    {PAPI_L1_STM, "L1_STM", "L1SM", "L1_STOR_MISSES"},
    {PAPI_L1_TCA, "L1_TCA", "L1A", "L1_ACCESSES"},
    {PAPI_L1_TCH, "L1_TCH", "L1H", "L1_HITS"},
    {PAPI_L1_TCM, "L1_TCM", "L1M", "L1_MISSES"},
    {PAPI_L1_TCR, "L1_TCR", "L1R", "L1_READS"},
    {PAPI_L1_TCW, "L1_TCW", "L1W", "L1_WRITES"},
    /* L2 Cache */
    {PAPI_L2_DCA, "L2_DCA", "L2DA", "L2_DATA_ACCESSES"},
    {PAPI_L2_DCH, "L2_DCH", "L2DH", "L2_DATA_HITS"},
    {PAPI_L2_DCM, "L2_DCM", "L2DM", "L2_DATA_MISSES"},
    {PAPI_L2_DCR, "L2_DCR", "L2DR", "L2_DATA_READS"},
    {PAPI_L2_DCW, "L2_DCW", "L2DW", "L2_DATA_WRITES"},
    {PAPI_L2_ICA, "L2_ICA", "L2IA", "L2_INST_ACCESSES"},
    {PAPI_L2_ICH, "L2_ICH", "L2IH", "L2_INST_HITS"},
    {PAPI_L2_ICM, "L2_ICM", "L2IM", "L2_INST_MISSES"},
    {PAPI_L2_ICR, "L2_ICR", "L2IR", "L2_INST_READS"},
    {PAPI_L2_ICW, "L2_ICW", "L2IW", "L2_INST_WRITES"},
    {PAPI_L2_LDM, "L2_LDM", "L2LM", "L2_LOAD_MISSES"},
    {PAPI_L2_STM, "L2_STM", "L2SM", "L2_STOR_MISSES"},
    {PAPI_L2_TCA, "L2_TCA", "L2A", "L2_ACCESSES"},
    {PAPI_L2_TCH, "L2_TCH", "L2H", "L2_HITS"},
    {PAPI_L2_TCM, "L2_TCM", "L2M", "L2_MISSES"},
    {PAPI_L2_TCR, "L2_TCR", "L2R", "L2_READS"},
    {PAPI_L2_TCW, "L2_TCW", "L2W", "L2_WRITES"},
    /* L3 Cache */
    {PAPI_L3_DCA, "L3_DCA", "L3DA", "L3_DATA_ACCESSES"},
    {PAPI_L3_DCH, "L3_DCH", "L3DH", "L3_DATA_HITS"},
    {PAPI_L3_DCM, "L3_DCM", "L3DM", "L3_DATA_MISSES"},
    {PAPI_L3_DCR, "L3_DCR", "L3DR", "L3_DATA_READS"},
    {PAPI_L3_DCW, "L3_DCW", "L3DW", "L3_DATA_WRITES"},
    {PAPI_L3_ICA, "L3_ICA", "L3IA", "L3_INST_ACCESSES"},
    {PAPI_L3_ICH, "L3_ICH", "L3IH", "L3_INST_HITS"},
    {PAPI_L3_ICM, "L3_ICM", "L3IM", "L3_INST_MISSES"},
    {PAPI_L3_ICR, "L3_ICR", "L3IR", "L3_INST_READS"},
    {PAPI_L3_ICW, "L3_ICW", "L3IW", "L3_INST_WRITES"},
    {PAPI_L3_LDM, "L3_LDM", "L3LM", "L3_LOAD_MISSES"},
    {PAPI_L3_STM, "L3_STM", "L3SM", "L3_STOR_MISSES"},
    {PAPI_L3_TCA, "L3_TCA", "L3A", "L3_ACCESSES"},
    {PAPI_L3_TCH, "L3_TCH", "L3H", "L3_HITS"},
    {PAPI_L3_TCM, "L3_TCM", "L3M", "L3_MISSES"},
    {PAPI_L3_TCR, "L3_TCR", "L3R", "L3_READS"},
    {PAPI_L3_TCW, "L3_TCW", "L3W", "L3_WRITES"},
    /* TLB */
    {PAPI_TLB_DM, "TLB_DM", "TBDM", "TLB_DATA_MISSES"},
    {PAPI_TLB_IM, "TLB_IM", "TBIM", "TLB_INST_MISSES"},
    {PAPI_TLB_SD, "TLB_SD", "TBSD", "TLB_SHOOTDOWNS"},
    {PAPI_TLB_TL, "TLB_TL", "TBM", "TLB_MISSES"},
    {0, NULL, NULL}
};                                     /*}}} */

static int lookup_event_by_str(
    char *str)
{                                      /*{{{ */
    size_t off;

    for (off = 0;
         event_lookup[off].papi != NULL &&
         strcasecmp(str, event_lookup[off].papi)
         && strcasecmp(str, event_lookup[off].nick)
         && strcasecmp(str, event_lookup[off].shortnick); ++off) ;
    return event_lookup[off].id;
}                                      /*}}} */

#if 0
static char *lookup_event_by_num(
    size_t num)
{                                      /*{{{ */
    size_t off;

    for (off = 0;
         event_lookup[off].papi != NULL && event_lookup[off].id != num;
         ++off) ;
    return event_lookup[off].shortnick;
}                                      /*}}} */
#endif

static void display_events(
    void)
{                                      /*{{{ */
    size_t i;

    printf("%-16s%-16s%-25s\n", "PAPI-name", "Short-Nickname", "Nickname");
    for (i = 0; event_lookup[i].papi != NULL; ++i) {
        printf("%-16s%-16s%-25s\n", event_lookup[i].papi,
               event_lookup[i].shortnick, event_lookup[i].nick);
    }
}                                      /*}}} */

void INTERNAL PtlInternalPAPIInit(
    void)
{                                      /*{{{ */
    int papi_ret;
    char errstring[PAPI_MAX_STR_LEN];
    /* first, initialize the library (compare the versions to make sure it will work) */
    if ((papi_ret = PAPI_library_init(PAPI_VER_CURRENT)) != PAPI_VER_CURRENT) {
        PAPI_perror(papi_ret, errstring, PAPI_MAX_STR_LEN);
        fprintf(stderr, "~%i~ Error initializing PAPI: %d reason: %s\n",
                (int)proc_number, papi_ret, errstring);
        exit(EXIT_FAILURE);
    }
    if (PAPI_thread_init(pthread_self) != PAPI_OK) {
        exit(EXIT_FAILURE);
    }
    if (PAPI_num_counters() < PAPI_OK) {
        fprintf(stderr, "~%i~ There are no PAPI counters available.\n",
                (int)proc_number);
        exit(EXIT_FAILURE);
    }
    /* figure out what counters we will be using */
    memset(papi_events, 0, sizeof(int) * MAX_PAPI_EVENTS);
    for (int i = 1; i <= MAX_PAPI_EVENTS; ++i) {
        char envariable[50];
        char *value;

        snprintf(envariable, 50, "PAPI_CTR_%i", i);
        value = getenv(envariable);
        if (value == NULL)
            break;
        if ((papi_events[i - 1] = lookup_event_by_str(value)) == 0) {
            fprintf(stderr, "~%i~ There is no event by the name \"%s\"\n",
                    (int)proc_number, value);
            numCounters = 0;
            break;
        }
        if (PAPI_query_event(papi_events[i - 1]) != PAPI_OK) {
            fprintf(stderr,
                    "~%i~ Event \"%s\" is unavailable on this machine.\n",
                    (int)proc_number, value);
            exit(EXIT_FAILURE);
        }
        numCounters = i;
    }
    if (numCounters == 0 && NULL != getenv("PAPI_CTR_HELP")) {
        display_events();
        fprintf(stderr,
                "~%i~ Counters may be specified in one of three styles:\n",
                (int)proc_number);
        fprintf(stderr, "~%i~ \t      Nickname: PAPI_CTR_1=CYCLES\n",
                (int)proc_number);
        fprintf(stderr, "~%i~ \tShort Nickname: PAPI_CTR_1=CYC\n",
                (int)proc_number);
        fprintf(stderr, "~%i~ \t     PAPI-name: PAPI_CTR_1=TOT_CYC\n",
                (int)proc_number);
        exit(EXIT_FAILURE);
    }
    papi_ctrs = malloc(sizeof(long long *) * NUM_INSTRUMENTED_FUNCS);
    assert(papi_ctrs);
    papi_measurements = malloc(sizeof(long long *) * NUM_INSTRUMENTED_FUNCS);
    assert(papi_measurements);
    papi_sums = malloc(sizeof(long long **) * NUM_INSTRUMENTED_FUNCS);
    assert(papi_sums);
    for (int i = 0; i < NUM_INSTRUMENTED_FUNCS; ++i) {
        papi_ctrs[i] = calloc(numCounters, sizeof(long long));
        assert(papi_ctrs[i]);
        papi_measurements[i] = calloc(NUM_SAVE_POINTS, sizeof(long long));
        assert(papi_measurements[i]);
        papi_sums[i] = malloc(sizeof(long long *) * NUM_SAVE_POINTS);
        assert(papi_sums[i]);
        for (int j = 0; j < NUM_SAVE_POINTS; ++j) {
            papi_sums[i][j] = calloc(numCounters, sizeof(long long));
            assert(papi_sums[i][j]);
        }
    }
    {
        char fname[50];
        snprintf(fname, 50, "papi.r%i.out", (int)proc_number);
        papi_out = fopen(fname, "w");
        assert(papi_out);
    }
}                                      /*}}} */

void INTERNAL PtlInternalPAPITeardown(
    void)
{                                      /*{{{ */
    PAPI_shutdown();
    for (int func = 0; func < NUM_INSTRUMENTED_FUNCS; ++func) {
        for (int savept = 0; savept < NUM_SAVE_POINTS; ++savept) {
            if (papi_measurements[func][savept] == 0) {
                free(papi_sums[func][savept]);
                continue;
            }
            fprintf(papi_out, "func%i pt%i ", func, savept);
            for (int ctr = 0; ctr < numCounters; ++ctr) {
                fprintf(papi_out, "%i: %f ", ctr,
                        ((double)papi_sums[func][savept][ctr]) /
                        ((double)papi_measurements[func][savept]));
            }
            fprintf(papi_out, "measured %lli\n",
                    papi_measurements[func][savept]);
            free(papi_sums[func][savept]);
        }
        free(papi_sums[func]);
        free(papi_ctrs[func]);
        free(papi_measurements[func]);
    }
    free(papi_sums);
    free(papi_ctrs);
    fclose(papi_out);
}                                      /*}}} */

void INTERNAL PtlInternalPAPIStartC(
    void)
{
    PAPI_start_counters(papi_events, numCounters);
}

void INTERNAL PtlInternalPAPISaveC(
    enum ptl_internal_papi_func func,
    int savept)
{
    PAPI_stop_counters(papi_ctrs[func], numCounters);
    for (int i = 0; i < numCounters; ++i) {
        papi_sums[func][savept][i] += papi_ctrs[func][i];
    }
    ++papi_measurements[func][savept];
    PAPI_start_counters(papi_events, numCounters);
}

void INTERNAL PtlInternalPAPIDoneC(
    enum ptl_internal_papi_func func,
    int savept)
{
    PAPI_stop_counters(papi_ctrs[func], numCounters);
    for (int i = 0; i < numCounters; ++i) {
        papi_sums[func][savept][i] += papi_ctrs[func][i];
    }
    ++papi_measurements[func][savept];
}
/* vim:set expandtab: */
