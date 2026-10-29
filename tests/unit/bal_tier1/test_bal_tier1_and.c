#include "bal_engine.h"
#include "bal_engine_flags.h"
#include "bal_memory.h"
#include "unity.h"
#include <string.h>

/* Engine-level tests: AND (shifted register), zero-register fields, W-forms. */

#define ARM64_RET_ENCODING     0xD65F03C0U
#define TEST_GUEST_MEMORY_SIZE 4096U

/* AND X8, X9, X10 */
#define AND_X8_X9_X10 0x8A0A0128U
/* AND X8, X9, X10, LSL #4 */
#define AND_X8_X9_X10_LSL4 0x8A0A1128U
/* AND X8, X9, XZR */
#define AND_X8_X9_XZR 0x8A1F0128U
/* AND X8, XZR, X10 */
#define AND_X8_XZR_X10 0x8A0A03E8U
/* AND X8, XZR, XZR */
#define AND_X8_XZR_XZR 0x8A1F03E8U
/* AND XZR, X9, X10 */
#define AND_XZR_X9_X10 0x8A0A01E8U
/* AND W8, W9, W10 */
#define AND_W8_W9_W10 0x0A0A0128U
/* AND W8, W9, W10, LSL #4 */
#define AND_W8_W9_W10_LSL4 0x0A0A1128U
/* AND W8, W9, W10, LSR #4 */
#define AND_W8_W9_W10_LSR4 0x0A4A1128U
/* AND W8, W9, W10, ASR #4 */
#define AND_W8_W9_W10_ASR4 0x0A8A1128U

static uint32_t bal_test_guest_memory[TEST_GUEST_MEMORY_SIZE / sizeof(uint32_t)];

void
setUp(void)
{
    (void)memset(bal_test_guest_memory, 0, sizeof(bal_test_guest_memory));
}

void
tearDown(void)
{
}

static void
run_single_guest_instruction(const uint32_t instruction, bal_cpu_t *BAL_RESTRICT cpu)
{
    bal_test_guest_memory[0] = instruction;
    bal_test_guest_memory[1] = ARM64_RET_ENCODING;

    bal_allocator_t allocator = {};
    bal_allocator_default_init(&allocator);

    bal_memory_interface_t memory_interface = {};
    TEST_ASSERT_EQUAL(BAL_SUCCESS,
                      bal_flat_translation_interface_init(&allocator,
                                                          &memory_interface,
                                                          bal_test_guest_memory,
                                                          sizeof(bal_test_guest_memory)));

    cpu->pc = 0ULL;

    bal_engine_t engine = {};
    TEST_ASSERT_EQUAL(BAL_SUCCESS, bal_engine_init(&engine, cpu, &allocator, &memory_interface));

    engine.flags |= BAL_ENGINE_FLAG_SINGLE_STEP;

    const bal_error_t run_status = bal_engine_run_thread(&engine);

    bal_engine_destroy(&engine);
    (void)bal_flat_translation_interface_destroy(&allocator, &memory_interface);

    TEST_ASSERT_EQUAL(BAL_SUCCESS, run_status);
}

static void
test_BalTier1And_CombinesBothSourceRegisters(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xAAAAAAAAAAAAAAAAULL;
    cpu.x[9]      = 0xFFFFFFFFFFFFFF00ULL;
    cpu.x[10]     = 0x00FF00FF00FF00FFULL;

    run_single_guest_instruction(AND_X8_X9_X10, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x00FF00FF00FF0000ULL, cpu.x[8]);
}

static void
test_BalTier1And_AppliesLeftShiftToSecondSource(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xDEADBEEFDEADBEEFULL;
    cpu.x[9]      = 0x00000000000000FFULL;
    cpu.x[10]     = 0x0000000000000001ULL;

    run_single_guest_instruction(AND_X8_X9_X10_LSL4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000010ULL, cpu.x[8]);
}

static void
test_BalTier1And_SecondSourceZeroRegisterClearsResult(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xAAAAAAAAAAAAAAAAULL;
    cpu.x[9]      = 0x0F0F0F0F0F0F0F0FULL;
    /* Slot 31 stores SP: a source field of 31 must read zero, never this. */
    cpu.x[31]     = 0xCAFEBABE00000000ULL;

    run_single_guest_instruction(AND_X8_X9_XZR, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000000ULL, cpu.x[8]);
    TEST_ASSERT_EQUAL_HEX64(0xCAFEBABE00000000ULL, cpu.x[31]);
}

static void
test_BalTier1And_FirstSourceZeroRegisterClearsResult(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xAAAAAAAAAAAAAAAAULL;
    cpu.x[10]     = 0x0123456789ABCDEFULL;
    cpu.x[31]     = 0xCAFEBABE00000000ULL;

    run_single_guest_instruction(AND_X8_XZR_X10, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000000ULL, cpu.x[8]);
    TEST_ASSERT_EQUAL_HEX64(0xCAFEBABE00000000ULL, cpu.x[31]);
}

static void
test_BalTier1And_BothSourcesZeroRegisterClearsResult(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xAAAAAAAAAAAAAAAAULL;
    cpu.x[31]     = 0xCAFEBABE00000000ULL;

    run_single_guest_instruction(AND_X8_XZR_XZR, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000000ULL, cpu.x[8]);
}

static void
test_BalTier1And_WriteToZeroRegisterIsDiscarded(void)
{
    bal_cpu_t cpu = {};
    /* Slot 31 stores SP; AND targets XZR, so the slot must be untouched. */
    cpu.x[9]      = 0xFFFFFFFFFFFFFF00ULL;
    cpu.x[10]     = 0x00FF00FF00FF00FFULL;
    cpu.x[31]     = 0xCAFEBABE00000000ULL;

    run_single_guest_instruction(AND_XZR_X9_X10, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0xCAFEBABE00000000ULL, cpu.x[31]);
}

static void
test_BalTier1And_ThirtyTwoBitResultClearsUpperHalf(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xAAAAAAAAAAAAAAAAULL;
    cpu.x[9]      = 0xFFFFFFFFFFFFFFFFULL;
    cpu.x[10]     = 0xDEAD000000BCF000ULL;

    run_single_guest_instruction(AND_W8_W9_W10, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000BCF000ULL, cpu.x[8]);
}

static void
test_BalTier1And_ThirtyTwoBitLeftShiftIgnoresUpperHalf(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xAAAAAAAAAAAAAAAAULL;
    cpu.x[9]      = 0xFFFFFFFFFFFFFFFFULL;
    cpu.x[10]     = 0x000000FF000000F0ULL;

    run_single_guest_instruction(AND_W8_W9_W10_LSL4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000F00ULL, cpu.x[8]);
}

static void
test_BalTier1And_ThirtyTwoBitLogicalShiftUsesThirtyTwoBits(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xAAAAAAAAAAAAAAAAULL;
    cpu.x[9]      = 0xFFFFFFFFFFFFFFFFULL;
    cpu.x[10]     = 0x000000FF00000F00ULL;

    run_single_guest_instruction(AND_W8_W9_W10_LSR4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x00000000000000F0ULL, cpu.x[8]);
}

static void
test_BalTier1And_ThirtyTwoBitArithmeticShiftExtendsSignBit(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xAAAAAAAAAAAAAAAAULL;
    cpu.x[9]      = 0xFFFFFFFFFFFFFFFFULL;
    cpu.x[10]     = 0x0000000080000010ULL;

    run_single_guest_instruction(AND_W8_W9_W10_ASR4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x00000000F8000001ULL, cpu.x[8]);
}

int
main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_BalTier1And_CombinesBothSourceRegisters);
    RUN_TEST(test_BalTier1And_AppliesLeftShiftToSecondSource);
    RUN_TEST(test_BalTier1And_SecondSourceZeroRegisterClearsResult);
    RUN_TEST(test_BalTier1And_FirstSourceZeroRegisterClearsResult);
    RUN_TEST(test_BalTier1And_BothSourcesZeroRegisterClearsResult);
    RUN_TEST(test_BalTier1And_WriteToZeroRegisterIsDiscarded);
    RUN_TEST(test_BalTier1And_ThirtyTwoBitResultClearsUpperHalf);
    RUN_TEST(test_BalTier1And_ThirtyTwoBitLeftShiftIgnoresUpperHalf);
    RUN_TEST(test_BalTier1And_ThirtyTwoBitLogicalShiftUsesThirtyTwoBits);
    RUN_TEST(test_BalTier1And_ThirtyTwoBitArithmeticShiftExtendsSignBit);
    return UNITY_END();
}

/*** end of file ***/
