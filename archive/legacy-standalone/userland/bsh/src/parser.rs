use alloc::string::String;
use alloc::vec::Vec;
use alloc::format;

use crate::cmdlets::CmdResult;
use crate::pipeline::run_pipeline;

pub fn execute(input: &str) -> Result<(), String> {
    let input = input.trim();
    if input.is_empty() {
        return Ok(());
    }

    // Split by pipeline operator |
    let segments: Vec<Vec<&str>> = input
        .split('|')
        .map(|seg| {
            seg.trim()
                .split_whitespace()
                .collect()
        })
        .collect();

    let output = run_pipeline(segments);

    if !output.is_empty() {
        crate::syscalls::write(1, output.as_bytes());
        crate::syscalls::write(1, b"\n");
    }

    Ok(())
}
