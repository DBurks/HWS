# Architectural Restrictions (Absolute System Rules)

- **Zero Runtime Virtual Overhead:** Never introduce `virtual` functions or runtime vtables inside high-frequency execution pipelines (like memory reads/writes or CPU clock stepping). 
- **Compile-Time Polymorphism:** Use **Policy-Based Design** via C++20 templates. All platform-specific widths (addresses, data words, register enums) must be passed through a static configuration traits tag (e.g., `template <typename Config>`).
- **Register Storage Layout:** Registers must be stored in a contiguous `std::array` (the Register File) indexed cleanly via explicit platform-specific type enums provided by the active policy.
- **Deterministic Time Slicing:** Real-time throttling must use 1 ms batch cycle-accumulation intervals against `std::chrono::steady_clock` absolute targets, rather than microsecond relative sleeps.