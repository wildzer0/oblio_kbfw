#include "check.h"
#include "profile.h"

#if LOOP_PROFILE

uint16_t profile_hook_now(void) { return 0; }

static void test_a_step_keeps_count_min_max_and_sum(void)
{
    profile_reset();
    CHECK(profile_step_count(3) == 0 && profile_step_min(3) == 0 && profile_step_max(3) == 0 && profile_step_sum(3) == 0);

    profile_step(3, 100);
    profile_step(3, 40);
    profile_step(3, 700);
    profile_step(4, 5);

    CHECK(profile_step_count(3) == 3);
    CHECK(profile_step_min(3) == 40 && profile_step_max(3) == 700);
    CHECK(profile_step_sum(3) == 840); // mean 280
    CHECK(profile_step_count(4) == 1 && profile_step_min(4) == 5);
}

static void test_the_sum_does_not_wrap_at_16_bits(void)
{
    profile_reset();

    for (int i = 0; i < 100; i++) {
        profile_step(1, 60000u);
    }

    CHECK(profile_step_sum(1) == 6000000ul && profile_step_count(1) == 100);
}

static void test_passes_fall_in_bands_of_double_length(void)
{
    profile_reset();
    profile_pass(0);      // band 0: < 400 counts (200 us)
    profile_pass(399);
    profile_pass(400);    // band 1
    profile_pass(1599);   // band 2: < 1600
    profile_pass(1600);   // band 3
    profile_pass(12800);  // band 6: < 25600 ... 12800 is the start of the band
    profile_pass(60000);  // the last band: longer than 12.8 ms

    CHECK(profile_band(0) == 2 && profile_band(1) == 1 && profile_band(2) == 1 && profile_band(3) == 1);
    CHECK(profile_band(6) == 1 && profile_band(7) == 1);
    CHECK(profile_band(4) == 0 && profile_band(5) == 0);
}

static void test_interrupt_time_adds_up_apart_from_the_steps(void)
{
    profile_reset();
    profile_isr(PROFILE_ISR_TIMER, 1000);
    profile_isr(PROFILE_ISR_TIMER, 2000);
    profile_isr(PROFILE_ISR_USB, 70);

    CHECK(profile_isr_sum(PROFILE_ISR_TIMER) == 3000 && profile_isr_sum(PROFILE_ISR_USB) == 70 && profile_isr_sum(PROFILE_ISR_UART) == 0);
}

static void test_a_bad_step_number_is_ignored(void)
{
    profile_reset();
    profile_step(PROFILE_STEPS, 10);
    profile_step(255, 10);
    CHECK(profile_step_count(PROFILE_STEPS) == 0);
}

int main(void)
{
    test_a_step_keeps_count_min_max_and_sum();
    test_the_sum_does_not_wrap_at_16_bits();
    test_passes_fall_in_bands_of_double_length();
    test_interrupt_time_adds_up_apart_from_the_steps();
    test_a_bad_step_number_is_ignored();
    return tests_result();
}

#else

int main(void)
{
    return 0;
}

#endif
