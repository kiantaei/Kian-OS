use crate::kprintln;
use crate::security::hash::sha256;

/* Password verification: SHA-256(salt + password) compared against a stored
 * hex hash. The plaintext password is NEVER stored. Default account: kian. */

pub const SALT: &[u8] = b"kian-os-v1";
pub const ACCOUNT_USER: &[u8] = b"kian";
/* sha256("kian-os-v1" + "kian") */
pub const ACCOUNT_HASH_HEX: &[u8] = b"cd895ecc90c11870a16b16fe516818945983040dbab345fe2cccef6680a503a3";

pub fn init() {
    kprintln!("[Kian] Auth: SHA-256(salt+password) enabled, account 'kian' stored hashed");
}

#[inline]
fn hex_nibble(v: u8) -> u8 {
    if v < 10 { b'0' + v } else { b'a' + v - 10 }
}

fn digest_matches_hex(digest: &[u8; 32], hex: &[u8]) -> bool {
    if hex.len() != 64 {
        return false;
    }
    for i in 0..32 {
        if hex_nibble(digest[i] >> 4) != hex[i * 2] || hex_nibble(digest[i] & 15) != hex[i * 2 + 1] {
            return false;
        }
    }
    true
}

/// ABI check used by kian_auth_check(). Compares SHA-256(SALT + pass) with
/// the stored hash of the default account.
pub fn verify(user: &[u8], pass: &[u8]) -> bool {
    if user != ACCOUNT_USER {
        return false;
    }
    if pass.is_empty() || pass.len() > 64 {
        return false;
    }
    let mut input = [0u8; 80]; // SALT.len() + 64
    input[..SALT.len()].copy_from_slice(SALT);
    input[SALT.len()..SALT.len() + pass.len()].copy_from_slice(pass);
    let digest = sha256(&input[..SALT.len() + pass.len()]);
    digest_matches_hex(&digest, ACCOUNT_HASH_HEX)
}

/* Legacy string wrapper kept for kernel-internal callers. */
pub fn authenticate(name: &str, password: &str) -> bool {
    verify(name.as_bytes(), password.as_bytes())
}
