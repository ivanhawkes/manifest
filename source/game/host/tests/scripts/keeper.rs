//! The host suite's mod script: a number kept by key, which a server's snapshot holds and its
//! recovery gives back. It needs only the store's two imports, so it declares them as the engine's
//! bindings do (Jettison::Script::ScriptHost::WriteRustModule).
#![no_std]

#[link(wasm_import_module = "jettison")]
unsafe extern "C" {
    #[link_name = "store"]
    fn jettison_store(key: *const u8, key_size: u32, data: *const u8, size: u32);
    #[link_name = "load"]
    fn jettison_load(key: *const u8, key_size: u32, buffer: *mut u8, capacity: u32) -> i32;
}

const KEY: &str = "count";

#[unsafe(export_name = "remember")]
pub extern "C" fn remember(value: i32) {
    let bytes = value.to_le_bytes();
    unsafe { jettison_store(KEY.as_ptr(), KEY.len() as u32, bytes.as_ptr(), bytes.len() as u32) }
}

/// The number kept, -1 for none.
#[unsafe(export_name = "recall")]
pub extern "C" fn recall() -> i32 {
    let mut bytes = [0u8; 4];
    let kept =
        unsafe { jettison_load(KEY.as_ptr(), KEY.len() as u32, bytes.as_mut_ptr(), bytes.len() as u32) };
    if kept == bytes.len() as i32 { i32::from_le_bytes(bytes) } else { -1 }
}

#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! {
    core::arch::wasm32::unreachable()
}
