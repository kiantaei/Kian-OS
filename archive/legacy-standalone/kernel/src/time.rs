use x86_64::instructions::port::Port;

/* CMOS RTC clock -> LOCAL time for Asia/Tehran.
 * RTC_IS_UTC = true: the CMOS clock holds UTC. Fixed offset +03:30
 * (Iran abolished DST in 2022), with proper day/month/year rollover. */

use crate::KianTime;

const RTC_IS_UTC: bool = true;
const TZ_OFFSET_SECS: i64 = 3 * 3600 + 30 * 60; // +03:30 fixed, NO DST

#[inline]
fn cmos_read(reg: u8) -> u8 {
    unsafe {
        Port::<u8>::new(0x70).write(reg & 0x7F);
        Port::<u8>::new(0x71).read()
    }
}

fn update_in_progress() -> bool {
    cmos_read(0x0A) & 0x80 != 0
}

fn wait_ready() {
    for _ in 0..1_000_000 {
        if !update_in_progress() {
            return;
        }
    }
}

fn bcd(v: u8) -> u8 {
    ((v & 0xF0) >> 4) * 10 + (v & 0x0F)
}

#[derive(Clone, Copy, PartialEq)]
struct RawClock {
    sec: i64,
    min: i64,
    hour: i64,
    day: i64,
    mon: i64,
    year: i64,
}

fn read_raw() -> RawClock {
    wait_ready();
    let statb = cmos_read(0x0B);
    let binary = statb & 0x04 != 0;
    let h24 = statb & 0x02 != 0;

    let sec = cmos_read(0x00);
    let min = cmos_read(0x02);
    let hour_raw = cmos_read(0x04);
    let day = cmos_read(0x07);
    let mon = cmos_read(0x08);
    let year = cmos_read(0x09);
    let century_raw = cmos_read(0x32);

    let conv = |v: u8| -> i64 {
        if binary { v as i64 } else { bcd(v) as i64 }
    };

    // 12h -> 24h
    let hour = if h24 {
        conv(hour_raw)
    } else {
        let pm = hour_raw & 0x80 != 0;
        let mut h = conv(hour_raw & 0x7F);
        if pm && h != 12 {
            h += 12;
        }
        if !pm && h == 12 {
            h = 0;
        }
        h
    };

    // century (usually BCD even in binary mode on many chipsets)
    let century = if century_raw >= 19 && century_raw <= 22 {
        century_raw as i64
    } else if binary {
        century_raw as i64
    } else {
        bcd(century_raw) as i64
    };

    RawClock {
        sec: conv(sec),
        min: conv(min),
        hour,
        day: conv(day),
        mon: conv(mon),
        year: century * 100 + conv(year),
    }
}

// Howard Hinnant's civil algorithms (pure integer, handles BCE years).
fn days_from_civil(y: i64, m: i64, d: i64) -> i64 {
    let y = if m <= 2 { y - 1 } else { y };
    let era = if y >= 0 { y } else { y - 399 } / 400;
    let yoe = y - era * 400;
    let mp = (m + 9) % 12;
    let doy = (153 * mp + 2) / 5 + d - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    era * 146097 + doe - 719468
}

fn civil_from_days(z: i64) -> (i64, i64, i64) {
    let z = z + 719468;
    let era = if z >= 0 { z } else { z - 146096 } / 146097;
    let doe = z - era * 146097;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    (if m <= 2 { y + 1 } else { y }, m, d)
}

/// Fill a kian_time_t with LOCAL time (Asia/Tehran, fixed UTC+03:30).
pub fn fill_local(out: &mut KianTime) {
    // read twice until stable (avoid torn reads across the UIP boundary)
    let mut a = read_raw();
    let mut tries = 0;
    loop {
        let b = read_raw();
        if a == b || tries >= 20 {
            break;
        }
        a = b;
        tries += 1;
    }
    let c = a;

    // sanity clamp against garbage RTC values
    if !(c.mon >= 1 && c.mon <= 12 && c.day >= 1 && c.day <= 31 && c.hour >= 0
        && c.hour < 24 && c.min >= 0 && c.min < 60 && c.sec >= 0 && c.sec < 60)
    {
        return; // keep the 1970 default rather than printing nonsense
    }

    // RTC is UTC -> add the fixed +03:30 offset, then roll day/month/year.
    let utc_days = days_from_civil(c.year, c.mon, c.day);
    let utc_secs = utc_days * 86400 + c.hour * 3600 + c.min * 60 + c.sec;
    let local_secs = if RTC_IS_UTC {
        utc_secs + TZ_OFFSET_SECS
    } else {
        utc_secs
    };
    let local_days = local_secs.div_euclid(86400);
    let local_rem = local_secs.rem_euclid(86400);
    let (y, m, d) = civil_from_days(local_days);

    // weekday: 1970-01-01 was a Thursday (4) with 0 = Sunday
    let weekday = (local_days + 4).rem_euclid(7);

    out.year = y as i32;
    out.month = m as i32;
    out.day = d as i32;
    out.hour = (local_rem / 3600) as i32;
    out.minute = ((local_rem / 60) % 60) as i32;
    out.second = (local_rem % 60) as i32;
    out.weekday = weekday as i32;
}
