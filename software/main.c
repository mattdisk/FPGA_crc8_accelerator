#include <stdint.h>
#include <stddef.h>

#include "xparameters.h"
#include "xil_io.h"
#include "xil_printf.h"

#include "sleep.h"

/* Hardware base addresses */
#define CRC8_BASE_ADDR   XPAR_CRC8_AXI_0_BASEADDR
#define GPIO_BASE_ADDR   XPAR_AXI_GPIO_0_BASEADDR
#define SEVENSEG_BASE_ADDR  XPAR_AXI_GPIO_1_BASEADDR

/* CRC-8 AXI register offsets */
#define CRC8_DATA_OFFSET      0x00U
#define CRC8_CONTROL_OFFSET   0x04U
#define CRC8_STATUS_OFFSET    0x08U
#define CRC8_RESULT_OFFSET    0x0CU

/* CRC-8 control bits */
#define CRC8_CONTROL_START    0x01U
#define CRC8_CONTROL_INIT     0x02U

/* CRC-8 status bits */
#define CRC8_STATUS_BUSY      0x01U
#define CRC8_STATUS_DONE      0x02U

/* AXI GPIO register offsets */
#define GPIO_DATA_OFFSET      0x00U
#define GPIO_TRI_OFFSET       0x04U

/* LED status values */
#define LED_PASS              0x0001U
#define LED_FAIL              0x0002U

/* Prevent an infinite software polling loop */
#define CRC8_POLL_LIMIT       1000000U

static uint8_t g_last_hardware_crc = 0U;

static const uint8_t hex_to_7seg[16] = {
    0x40U, /* 0 */
    0x79U, /* 1 */
    0x24U, /* 2 */
    0x30U, /* 3 */
    0x19U, /* 4 */
    0x12U, /* 5 */
    0x02U, /* 6 */
    0x78U, /* 7 */
    0x00U, /* 8 */
    0x10U, /* 9 */
    0x08U, /* A */
    0x03U, /* b */
    0x46U, /* C */
    0x21U, /* d */
    0x06U, /* E */
    0x0EU  /* F */
};

static void sevenseg_write_digit(
    uint8_t anode_mask,
    uint8_t segment_pattern
)
{
    uint32_t value;

    /*
     * Bit 7 controls the decimal point.
     * Set it to 1 to keep the decimal point off.
     */
    segment_pattern |= 0x80U;

    value =
        ((uint32_t)anode_mask << 8) |
        (uint32_t)segment_pattern;

    Xil_Out32(
        SEVENSEG_BASE_ADDR + GPIO_DATA_OFFSET,
        value
    );
}


static void sevenseg_refresh_crc_once(uint8_t crc)
{
    uint8_t high_nibble;
    uint8_t low_nibble;

    high_nibble = (crc >> 4) & 0x0FU;
    low_nibble  = crc & 0x0FU;

    /*
     * AN1 active, all other digits disabled.
     * Active-low anodes: 1101 = 0xD.
     */
    sevenseg_write_digit(
        0x0DU,
        hex_to_7seg[high_nibble]
    );

    usleep(1000);

    /*
     * AN0 active, all other digits disabled.
     * Active-low anodes: 1110 = 0xE.
     */
    sevenseg_write_digit(
        0x0EU,
        hex_to_7seg[low_nibble]
    );

    usleep(1000);
}


static void sevenseg_display_crc_for_2_second(uint8_t crc)
{
    for (int i = 0; i < 1000; ++i) {
        sevenseg_refresh_crc_once(crc);
    }
}


static void sevenseg_blank(void)
{
    /*
     * All segments off and all four digits disabled.
     */
    Xil_Out32(
        SEVENSEG_BASE_ADDR + GPIO_DATA_OFFSET,
        0x00000FFFU
    );
}

static uint8_t crc8_software(const uint8_t *data, size_t length)
{
    uint8_t crc = 0x00U;

    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];

        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x80U) != 0U) {
                crc = (uint8_t)((crc << 1) ^ 0x07U);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }

    return crc;
}


static void crc8_hardware_init(void)
{
    Xil_Out32(
        CRC8_BASE_ADDR + CRC8_CONTROL_OFFSET,
        CRC8_CONTROL_INIT
    );
}


static int crc8_hardware_write_byte(uint8_t data)
{
    uint32_t status;

    Xil_Out32(
        CRC8_BASE_ADDR + CRC8_DATA_OFFSET,
        (uint32_t)data
    );

    Xil_Out32(
        CRC8_BASE_ADDR + CRC8_CONTROL_OFFSET,
        CRC8_CONTROL_START
    );

    for (uint32_t count = 0U; count < CRC8_POLL_LIMIT; ++count) {

        status = Xil_In32(
            CRC8_BASE_ADDR + CRC8_STATUS_OFFSET
        );

        if ((status & CRC8_STATUS_DONE) != 0U) {
            return 1;
        }
    }

    return 0;
}


static int crc8_hardware_process(
    const uint8_t *data,
    size_t length,
    int initialize_first,
    uint8_t *result
)
{
    if (initialize_first != 0) {
        crc8_hardware_init();
    }

    for (size_t i = 0; i < length; ++i) {

        if (!crc8_hardware_write_byte(data[i])) {
            return 0;
        }
    }

    *result = (uint8_t)(
        Xil_In32(CRC8_BASE_ADDR + CRC8_RESULT_OFFSET)
        & 0xFFU
    );

    g_last_hardware_crc = *result;

    return 1;
}


static int crc8_hardware(
    const uint8_t *data,
    size_t length,
    uint8_t *result
)
{
    return crc8_hardware_process(
        data,
        length,
        1,
        result
    );
}


static void print_data(
    const uint8_t *data,
    size_t length
)
{
    for (size_t i = 0; i < length; ++i) {
        xil_printf("%02X ", data[i]);
    }
}


static int run_normal_test(
    int test_number,
    const char *name,
    const uint8_t *data,
    size_t length,
    int expected_crc
)
{
    uint8_t software_crc;
    uint8_t hardware_crc = 0U;
    int hardware_ok;
    int pass;

    software_crc = crc8_software(data, length);

    hardware_ok = crc8_hardware(
        data,
        length,
        &hardware_crc
    );

    xil_printf("\r\nTest %d: %s\r\n", test_number, name);

    xil_printf("Data: ");
    print_data(data, length);
    xil_printf("\r\n");

    if (expected_crc >= 0) {
        xil_printf(
            "Expected CRC: 0x%02X\r\n",
            (unsigned int)expected_crc
        );
    }

    xil_printf(
        "Software CRC: 0x%02X\r\n",
        software_crc
    );

    if (hardware_ok) {

        xil_printf(
            "Hardware CRC: 0x%02X\r\n",
            hardware_crc
        );

    } else {

        xil_printf("Hardware CRC: TIMEOUT\r\n");
    }

    pass =
        hardware_ok &&
        (software_crc == hardware_crc);

    if (expected_crc >= 0) {
        pass =
            pass &&
            (software_crc == (uint8_t)expected_crc);
    }

    if (pass) {
        xil_printf("Result: PASS\r\n");
        return 1;
    }

    xil_printf("Result: FAIL\r\n");
    return 0;
}


static int run_reset_before_test(int test_number)
{
    static const uint8_t pollution[] = {
        0xAA, 0x55, 0x12, 0x34
    };

    static const uint8_t target[] = {
        0x31
    };

    uint8_t ignored_crc = 0U;
    uint8_t hardware_crc = 0U;
    uint8_t software_crc;
    int hardware_ok;

    xil_printf(
        "\r\nTest %d: Reset before calculation\r\n",
        test_number
    );

    /*
     * First place the CRC hardware into a non-zero state.
     */
    crc8_hardware_init();

    hardware_ok = crc8_hardware_process(
        pollution,
        sizeof(pollution),
        0,
        &ignored_crc
    );

    if (!hardware_ok) {
        xil_printf("Pollution phase: TIMEOUT\r\n");
        xil_printf("Result: FAIL\r\n");
        return 0;
    }

    /*
     * Reset the CRC state before processing the target byte.
     */
    crc8_hardware_init();

    hardware_ok = crc8_hardware_process(
        target,
        sizeof(target),
        0,
        &hardware_crc
    );

    software_crc = crc8_software(
        target,
        sizeof(target)
    );

    xil_printf("Target data: 31\r\n");
    xil_printf("Expected CRC: 0x97\r\n");
    xil_printf(
        "Software CRC: 0x%02X\r\n",
        software_crc
    );

    if (hardware_ok) {
        xil_printf(
            "Hardware CRC: 0x%02X\r\n",
            hardware_crc
        );
    } else {
        xil_printf("Hardware CRC: TIMEOUT\r\n");
    }

    if (
        hardware_ok &&
        software_crc == 0x97U &&
        hardware_crc == software_crc
    ) {
        xil_printf("Result: PASS\r\n");
        return 1;
    }

    xil_printf("Result: FAIL\r\n");
    return 0;
}


static int run_reset_between_test(int test_number)
{
    static const uint8_t message_a[] = {
        'A', 'B', 'C'
    };

    static const uint8_t message_b[] = {
        0x12, 0x34, 0x56, 0x78
    };

    uint8_t sw_a;
    uint8_t sw_b;
    uint8_t hw_a = 0U;
    uint8_t hw_b = 0U;

    int ok_a;
    int ok_b;

    xil_printf(
        "\r\nTest %d: Reset between messages\r\n",
        test_number
    );

    sw_a = crc8_software(
        message_a,
        sizeof(message_a)
    );

    crc8_hardware_init();

    ok_a = crc8_hardware_process(
        message_a,
        sizeof(message_a),
        0,
        &hw_a
    );

    /*
     * Explicitly reset CRC state between the two messages.
     */
    crc8_hardware_init();

    sw_b = crc8_software(
        message_b,
        sizeof(message_b)
    );

    ok_b = crc8_hardware_process(
        message_b,
        sizeof(message_b),
        0,
        &hw_b
    );

    xil_printf("Message A: ");
    print_data(message_a, sizeof(message_a));
    xil_printf("\r\n");

    xil_printf(
        "A Software CRC: 0x%02X\r\n",
        sw_a
    );

    if (ok_a) {
        xil_printf(
            "A Hardware CRC: 0x%02X\r\n",
            hw_a
        );
    } else {
        xil_printf("A Hardware CRC: TIMEOUT\r\n");
    }

    xil_printf("Message B: ");
    print_data(message_b, sizeof(message_b));
    xil_printf("\r\n");

    xil_printf(
        "B Software CRC: 0x%02X\r\n",
        sw_b
    );

    if (ok_b) {
        xil_printf(
            "B Hardware CRC: 0x%02X\r\n",
            hw_b
        );
    } else {
        xil_printf("B Hardware CRC: TIMEOUT\r\n");
    }

    if (
        ok_a &&
        ok_b &&
        sw_a == hw_a &&
        sw_b == hw_b
    ) {
        xil_printf("Result: PASS\r\n");
        return 1;
    }

    xil_printf("Result: FAIL\r\n");
    return 0;
}


static int run_consecutive_test(int test_number)
{
    static const uint8_t message_a[] = {
        0x12, 0x34
    };

    static const uint8_t message_b[] = {
        0xDE, 0xAD, 0xBE, 0xEF
    };

    uint8_t sw_a;
    uint8_t sw_b;
    uint8_t hw_a = 0U;
    uint8_t hw_b = 0U;

    int ok_a;
    int ok_b;

    xil_printf(
        "\r\nTest %d: Consecutive independent CRC operations\r\n",
        test_number
    );

    /*
     * Run two complete CRC calculations back-to-back
     * without resetting or reprogramming the processor.
     */
    sw_a = crc8_software(
        message_a,
        sizeof(message_a)
    );

    ok_a = crc8_hardware(
        message_a,
        sizeof(message_a),
        &hw_a
    );

    sw_b = crc8_software(
        message_b,
        sizeof(message_b)
    );

    ok_b = crc8_hardware(
        message_b,
        sizeof(message_b),
        &hw_b
    );

    xil_printf(
        "Operation A SW/HW: 0x%02X / 0x%02X\r\n",
        sw_a,
        hw_a
    );

    xil_printf(
        "Operation B SW/HW: 0x%02X / 0x%02X\r\n",
        sw_b,
        hw_b
    );

    if (
        ok_a &&
        ok_b &&
        sw_a == hw_a &&
        sw_b == hw_b
    ) {
        xil_printf("Result: PASS\r\n");
        return 1;
    }

    xil_printf("Result: FAIL\r\n");
    return 0;
}


int main(void)
{
    static const uint8_t test1[] = "123456789";

    static const uint8_t test2[] = {
        0x31
    };

    static const uint8_t test3[] = {
        0x00
    };

    static const uint8_t test4[] = {
        0xFF
    };

    static const uint8_t test5[] = {
        0x00, 0x00, 0x00, 0x00
    };

    static const uint8_t test6[] = {
        0xFF, 0xFF, 0xFF, 0xFF
    };

    static const uint8_t test7[] = {
        0xAA, 0x55, 0xAA, 0x55
    };

    static const uint8_t test8[] = {
        0x5A, 0x5A, 0x5A, 0x5A
    };

    static const uint8_t test9[] = {
        'A', 'B', 'C'
    };

    int passed = 0;
    const int total_tests = 12;

    /*
     * Configure all AXI GPIO pins as outputs.
     */
    Xil_Out32(
        GPIO_BASE_ADDR + GPIO_TRI_OFFSET,
        0x00000000U
    );

    sevenseg_blank();

    /*
     * Turn all LEDs off before testing.
     */
    Xil_Out32(
        GPIO_BASE_ADDR + GPIO_DATA_OFFSET,
        0x00000000U
    );

    xil_printf("\r\n");
    xil_printf("============================================\r\n");
    xil_printf(" CRC-8 Hardware Accelerator Final Test Suite\r\n");
    xil_printf("============================================\r\n");

    passed += run_normal_test(
        1,
        "Standard vector 123456789",
        test1,
        sizeof(test1) - 1U,
        0xF4
    );

    sevenseg_display_crc_for_2_second(
        g_last_hardware_crc
    );

    passed += run_normal_test(
        2,
        "Normal single byte 0x31",
        test2,
        sizeof(test2),
        0x97
    );

    sevenseg_display_crc_for_2_second(
        g_last_hardware_crc
    );

    passed += run_normal_test(
        3,
        "Single byte 0x00",
        test3,
        sizeof(test3),
        -1
    );

    sevenseg_display_crc_for_2_second(
        g_last_hardware_crc
    );

    passed += run_normal_test(
        4,
        "Single byte 0xFF",
        test4,
        sizeof(test4),
        -1
    );
    
    sevenseg_display_crc_for_2_second(
        g_last_hardware_crc
    );

    passed += run_normal_test(
        5,
        "All-zero data",
        test5,
        sizeof(test5),
        -1
    );

    sevenseg_display_crc_for_2_second(
        g_last_hardware_crc
    );

    passed += run_normal_test(
        6,
        "All-one data",
        test6,
        sizeof(test6),
        -1
    );

    sevenseg_display_crc_for_2_second(
        g_last_hardware_crc
    );

    passed += run_normal_test(
        7,
        "Alternating 0xAA / 0x55 pattern",
        test7,
        sizeof(test7),
        -1
    );

    sevenseg_display_crc_for_2_second(
        g_last_hardware_crc
    );

    passed += run_normal_test(
        8,
        "Repeated bytes",
        test8,
        sizeof(test8),
        -1
    );

    sevenseg_display_crc_for_2_second(
        g_last_hardware_crc
    );

    passed += run_normal_test(
        9,
        "Multi-byte message ABC",
        test9,
        sizeof(test9),
        -1
    );

    sevenseg_display_crc_for_2_second(
        g_last_hardware_crc
    );

    passed += run_reset_before_test(10);

    sevenseg_display_crc_for_2_second(
        g_last_hardware_crc
    );

    passed += run_reset_between_test(11);

    sevenseg_display_crc_for_2_second(
        g_last_hardware_crc
    );

    passed += run_consecutive_test(12);

    xil_printf("\r\n");
    xil_printf("============================================\r\n");

    xil_printf(
        "Passed: %d / %d\r\n",
        passed,
        total_tests
    );

    if (passed == total_tests) {

        xil_printf(
            "FINAL RESULT: ALL TESTS PASSED\r\n"
        );

        Xil_Out32(
            GPIO_BASE_ADDR + GPIO_DATA_OFFSET,
            LED_PASS
        );

    } else {

        xil_printf(
            "FINAL RESULT: TEST FAILURE\r\n"
        );

        Xil_Out32(
            GPIO_BASE_ADDR + GPIO_DATA_OFFSET,
            LED_FAIL
        );
    }

    xil_printf("============================================\r\n");

    while (1) {
        sevenseg_refresh_crc_once(
            g_last_hardware_crc
        );
    }

    return 0;
}