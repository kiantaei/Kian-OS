use alloc::string::String;
use alloc::vec::Vec;
use alloc::format;

use crate::cmdlets::{Object, Pipeline, CmdResult, dispatch, format_table, format_list, where_object, select_object, sort_object, for_each_object, measure_object, convert_to_json};

pub fn run_pipeline(segments: Vec<Vec<&str>>) -> String {
    if segments.is_empty() {
        return String::new();
    }

    // First segment produces initial pipeline.
    let first = &segments[0];
    let cmd = first[0];
    let args = &first[1..];
    let mut result = match dispatch(cmd, args) {
        CmdResult::Pipeline(p) => p,
        CmdResult::Text(t) => return t,
        CmdResult::None => return String::new(),
    };

    // Subsequent segments are pipeline operators.
    for segment in segments.iter().skip(1) {
        if segment.is_empty() { continue; }
        let op = segment[0];
        let args = &segment[1..];
        match op {
            "Where-Object" => {
                let predicate = args.first().copied().unwrap_or("");
                result = where_object(result, predicate);
            }
            "Select-Object" => {
                let property = args.first().copied().unwrap_or("");
                result = select_object(result, property);
            }
            "Sort-Object" => {
                let ascending = !args.contains(&"-Descending");
                result = sort_object(result, ascending);
            }
            "ForEach-Object" => {
                let action = args.first().copied().unwrap_or("");
                result = for_each_object(result, action);
            }
            "Measure-Object" => {
                result = measure_object(result);
            }
            "Format-Table" => {
                return format_table(&result);
            }
            "Format-List" => {
                return format_list(&result);
            }
            "ConvertTo-Json" => {
                return convert_to_json(&result);
            }
            _ => {
                return format!("Unknown pipeline operator: {}\n", op);
            }
        }
    }

    // Default output: Format-Table.
    format_table(&result)
}
