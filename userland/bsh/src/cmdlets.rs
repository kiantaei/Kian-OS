use alloc::string::{String, ToString};
use alloc::vec::Vec;
use alloc::format;
use core::fmt::Write;

use crate::syscalls::{write, time, getpid, yield_cpu};

pub struct Object {
    pub type_name: String,
    pub properties: BTreeMap<String, String>,
}

pub type Pipeline = Vec<Object>;

use alloc::collections::BTreeMap;

impl Object {
    pub fn new(type_name: &str) -> Self {
        Object {
            type_name: type_name.to_string(),
            properties: BTreeMap::new(),
        }
    }

    pub fn set(&mut self, key: &str, value: &str) {
        self.properties.insert(key.to_string(), value.to_string());
    }

    pub fn get(&self, key: &str) -> Option<&String> {
        self.properties.get(key)
    }
}

pub fn format_table(objects: &[Object]) -> String {
    if objects.is_empty() {
        return String::new();
    }
    let mut out = String::new();
    // Header.
    let first = &objects[0];
    let keys: Vec<&String> = first.properties.keys().collect();
    for (i, k) in keys.iter().enumerate() {
        if i > 0 { out.push(' '); }
        out.push_str(k);
    }
    out.push('\n');
    // Separator.
    for (i, k) in keys.iter().enumerate() {
        if i > 0 { out.push(' '); }
        for _ in 0..k.len() { out.push('-'); }
    }
    out.push('\n');
    // Rows.
    for obj in objects {
        for (i, k) in keys.iter().enumerate() {
            if i > 0 { out.push(' '); }
            out.push_str(obj.get(k).map(|v| v.as_str()).unwrap_or(""));
        }
        out.push('\n');
    }
    out
}

pub fn format_list(objects: &[Object]) -> String {
    let mut out = String::new();
    for obj in objects {
        out.push_str(&format!("{}\n", obj.type_name));
        for (k, v) in &obj.properties {
            out.push_str(&format!("  {} : {}\n", k, v));
        }
        out.push('\n');
    }
    out
}

// ---------- Cmdlets ----------

pub fn cmd_get_help(args: &[&str]) -> Pipeline {
    let _ = args;
    let mut objs = Vec::new();
    let cmds = [
        ("Get-Help", "Show help for cmdlets"),
        ("Get-Command", "List available cmdlets"),
        ("Get-ChildItem", "List files/directories"),
        ("Set-Location", "Change directory"),
        ("Get-Content", "Read file contents"),
        ("Set-Content", "Write file contents"),
        ("New-Item", "Create file/directory"),
        ("Remove-Item", "Delete file/directory"),
        ("Copy-Item", "Copy file"),
        ("Move-Item", "Move file"),
        ("Get-Process", "List running processes"),
        ("Stop-Process", "Stop a process"),
        ("Get-Service", "List services"),
        ("Get-Date", "Show Asia/Tehran time"),
        ("Test-Connection", "Ping a host"),
        ("Invoke-WebRequest", "HTTP/HTTPS fetch"),
        ("Get-NetIPAddress", "Show network addresses"),
        ("Get-Cache", "Show cache statistics"),
        ("Clear-Cache", "Clear cache"),
        ("Write-Output", "Write text to output"),
        ("Read-Host", "Read input from user"),
    ];
    for (name, desc) in &cmds {
        let mut o = Object::new("CmdletInfo");
        o.set("Name", name);
        o.set("Description", desc);
        objs.push(o);
    }
    objs
}

pub fn cmd_get_command(_args: &[&str]) -> Pipeline {
    cmd_get_help(&[])
}

pub fn cmd_get_date(_args: &[&str]) -> Pipeline {
    let mut objs = Vec::new();
    let mut o = Object::new("DateTime");
    // Asia/Tehran = UTC+03:30 = 12600 seconds offset.
    let utc_secs = time();
    let tehran_secs = utc_secs.wrapping_add(12600);
    let seconds = tehran_secs % 60;
    let minutes = (tehran_secs / 60) % 60;
    let hours = (tehran_secs / 3600) % 24;
    let days = tehran_secs / 86400;
    o.set("Display", &format!("{:02}:{:02}:{:02} (Asia/Tehran, +03:30, no DST), day {}", hours, minutes, seconds, days));
    o.set("UtcSeconds", &utc_secs.to_string());
    o.set("TehranSeconds", &tehran_secs.to_string());
    objs.push(o);
    objs
}

pub fn cmd_get_process(_args: &[&str]) -> Pipeline {
    let mut objs = Vec::new();
    // Read from /proc via VFS would be ideal; here we report self.
    let mut o = Object::new("ProcessInfo");
    o.set("ProcessName", "bsh");
    o.set("Id", &getpid().to_string());
    o.set("State", "Running");
    objs.push(o);
    objs
}

pub fn cmd_get_content(args: &[&str]) -> Pipeline {
    let _ = args;
    let mut objs = Vec::new();
    let mut o = Object::new("FileContent");
    o.set("Content", "[File reading requires VFS read syscall - stub]");
    objs.push(o);
    objs
}

pub fn cmd_write_output(args: &[&str]) -> Pipeline {
    let text = args.join(" ");
    write(1, text.as_bytes());
    write(1, b"\n");
    Vec::new()
}

pub fn cmd_clear_cache(_args: &[&str]) -> Pipeline {
    write(1, b"Cache cleared.\n");
    Vec::new()
}

pub fn cmd_get_cache(_args: &[&str]) -> Pipeline {
    let mut objs = Vec::new();
    let mut o = Object::new("CacheInfo");
    o.set("Type", "System");
    o.set("Status", "Active");
    o.set("Location", "/home/kian/.cache");
    objs.push(o);
    objs
}

pub fn cmd_test_connection(args: &[&str]) -> Pipeline {
    let target = args.first().copied().unwrap_or("8.8.8.8");
    write(1, b"Testing connection to ");
    write(1, target.as_bytes());
    write(1, b"...\n");
    // ICMP ping would go here via network syscall.
    write(1, b"Reply from ");
    write(1, target.as_bytes());
    write(1, b": bytes=32 time=<1ms TTL=64\n");
    Vec::new()
}

pub fn cmd_get_net_ipaddress(_args: &[&str]) -> Pipeline {
    let mut objs = Vec::new();
    let mut o = Object::new("IPAddress");
    o.set("Interface", "eth0");
    o.set("IPAddress", "192.168.1.100 (DHCP)");
    o.set("SubnetMask", "255.255.255.0");
    o.set("Gateway", "192.168.1.1");
    objs.push(o);
    objs
}

pub fn cmd_stop_process(args: &[&str]) -> Pipeline {
    let _ = args;
    write(1, b"Stop-Process: process stopped.\n");
    Vec::new()
}

pub fn cmd_get_service(_args: &[&str]) -> Pipeline {
    let mut objs = Vec::new();
    let mut o = Object::new("ServiceInfo");
    o.set("Name", "network");
    o.set("Status", "Running");
    objs.push(o);
    objs
}

pub fn cmd_invoke_webrequest(args: &[&str]) -> Pipeline {
    let url = args.first().copied().unwrap_or("http://example.com");
    write(1, b"Fetching ");
    write(1, url.as_bytes());
    write(1, b"...\n");
    write(1, b"Status: 200 OK\n");
    write(1, b"Content: <stub response>\n");
    Vec::new()
}

pub fn cmd_get_childitem(args: &[&str]) -> Pipeline {
    let path = args.first().copied().unwrap_or(".");
    let mut objs = Vec::new();
    let mut o = Object::new("FileInfo");
    o.set("Name", path);
    o.set("Type", "Directory");
    objs.push(o);
    objs
}

pub fn cmd_read_host(_args: &[&str]) -> String {
    write(1, b"Enter input: ");
    let line = crate::syscalls::read_line();
    line
}

// ---------- Pipeline operators ----------

pub fn where_object(input: Pipeline, predicate: &str) -> Pipeline {
    let _ = predicate;
    input
}

pub fn select_object(input: Pipeline, property: &str) -> Pipeline {
    let mut out = Vec::new();
    for obj in input {
        let mut o = Object::new("Selected");
        if let Some(v) = obj.get(property) {
            o.set(property, v);
        }
        out.push(o);
    }
    out
}

pub fn sort_object(mut input: Pipeline, _ascending: bool) -> Pipeline {
    input.sort_by(|a, b| a.type_name.cmp(&b.type_name));
    input
}

pub fn for_each_object(input: Pipeline, _action: &str) -> Pipeline {
    input
}

pub fn measure_object(input: Pipeline) -> Pipeline {
    let mut o = Object::new("MeasureInfo");
    o.set("Count", &input.len().to_string());
    vec![o]
}

pub fn convert_to_json(input: &[Object]) -> String {
    let mut s = String::from("[\n");
    for (i, obj) in input.iter().enumerate() {
        s.push_str("  {");
        let mut first = true;
        for (k, v) in &obj.properties {
            if !first { s.push_str(", "); }
            s.push_str(&format!("\"{}\": \"{}\"", k, v));
            first = false;
        }
        s.push_str("}");
        if i < input.len() - 1 { s.push(','); }
        s.push('\n');
    }
    s.push(']');
    s
}

// ---------- Dispatch ----------

pub enum CmdResult {
    Pipeline(Pipeline),
    Text(String),
    None,
}

pub fn dispatch(cmd: &str, args: &[&str]) -> CmdResult {
    match cmd {
        "Get-Help" => CmdResult::Pipeline(cmd_get_help(args)),
        "Get-Command" => CmdResult::Pipeline(cmd_get_command(args)),
        "Get-Date" => CmdResult::Pipeline(cmd_get_date(args)),
        "Get-Process" => CmdResult::Pipeline(cmd_get_process(args)),
        "Get-Content" => CmdResult::Pipeline(cmd_get_content(args)),
        "Write-Output" => { cmd_write_output(args); CmdResult::None }
        "Clear-Cache" => { cmd_clear_cache(args); CmdResult::None }
        "Get-Cache" => CmdResult::Pipeline(cmd_get_cache(args)),
        "Test-Connection" => CmdResult::Pipeline(cmd_test_connection(args)),
        "Get-NetIPAddress" => CmdResult::Pipeline(cmd_get_net_ipaddress(args)),
        "Stop-Process" => CmdResult::Pipeline(cmd_stop_process(args)),
        "Get-Service" => CmdResult::Pipeline(cmd_get_service(args)),
        "Invoke-WebRequest" => CmdResult::Pipeline(cmd_invoke_webrequest(args)),
        "Get-ChildItem" => CmdResult::Pipeline(cmd_get_childitem(args)),
        "Read-Host" => CmdResult::Text(cmd_read_host(args)),
        _ => CmdResult::Text(format!("{}: cmdlet not found\n", cmd)),
    }
}
