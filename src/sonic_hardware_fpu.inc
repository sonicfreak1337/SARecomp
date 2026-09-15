// Included only in the authenticated Linux FPU implementation's private scope.
// The existing instruction/mode checks and complete integer/fault paths remain.
// Accept a strict subset of the original integer helpers' ordinary results so
// that preserving MXCSR also matches their host-state behavior inside epochs.
[[nodiscard]] SONIC_FPU_RUNTIME_INLINE bool sonic_try_hardware_single(
    std::uint32_t n, std::uint32_t m, FpuBinaryOperation operation,
    std::uint8_t rounding_mode, SingleBinaryResult& result) noexcept {
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
    const auto n_magnitude = n & 0x7FFFFFFFu;
    const auto m_magnitude = m & 0x7FFFFFFFu;
    if (n_magnitude >= 0x7F800000u || m_magnitude >= 0x7F800000u ||
        (n_magnitude != 0u && n_magnitude < 0x00800000u) ||
        (m_magnitude != 0u && m_magnitude < 0x00800000u) ||
        (operation == FpuBinaryOperation::Divide && m_magnitude == 0u))
        return false;
    if (operation != FpuBinaryOperation::Add && operation != FpuBinaryOperation::Subtract &&
        operation != FpuBinaryOperation::Multiply && operation != FpuBinaryOperation::Divide)
        return false;

    auto left = _mm_set_ss(std::bit_cast<float>(n));
    const auto right = _mm_set_ss(std::bit_cast<float>(m));
    unsigned saved, status;
    // Ordered volatile assembly makes the FP status boundaries explicit.
    // No FMA, reassociation, FTZ/DAZ, x87 arithmetic or host exception escapes.
    __asm__ volatile("stmxcsr %0" : "=m"(saved) : : "memory");
    const auto requested = requested_host_control(saved, rounding_mode);
    __asm__ volatile("ldmxcsr %0" : : "m"(requested) : "memory");
    switch (operation) {
    case FpuBinaryOperation::Add:
        __asm__ volatile("addss %1, %0" : "+x"(left) : "x"(right)); break;
    case FpuBinaryOperation::Subtract:
        __asm__ volatile("subss %1, %0" : "+x"(left) : "x"(right)); break;
    case FpuBinaryOperation::Multiply:
        __asm__ volatile("mulss %1, %0" : "+x"(left) : "x"(right)); break;
    case FpuBinaryOperation::Divide:
        __asm__ volatile("divss %1, %0" : "+x"(left) : "x"(right)); break;
    }
    __asm__ volatile("stmxcsr %0\n\tldmxcsr %1" : "=m"(status) : "m"(saved) : "memory");
    const auto bits = std::bit_cast<std::uint32_t>(_mm_cvtss_f32(left));
    const auto exponent = (bits >> 23u) & 0xFFu;
    if ((status & 0x1Fu) != 0u ||
        ((bits & 0x7FFFFFFFu) != 0u && (exponent < 2u || exponent > 252u)))
        return false;
    result.bits = bits;
    result.causes = (status & 0x20u) != 0u ? fpscr_cause_inexact_mask : 0u;
    return true;
#else
    return false;
#endif
}
