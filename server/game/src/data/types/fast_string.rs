use anyhow::{bail, Result};
use bytebuffer::{ByteBuffer, ByteReader};
use std::fmt;

use crate::data::bytebufferext::*;

/// `FastString` is a string class more powerful than `&str` but doesn't use heap allocation like `String`.
/// When encoding or decoding into a byte buffer of any kind, the encoded form is identical to a normal `String`,
/// and they can be converted between each other interchangably with `.try_into()` or `.try_from()`
#[derive(Clone)]
pub struct FastString<const N: usize> {
    buffer: [u8; N],
    len: usize,
}

impl<const N: usize> FastString<N> {
    pub fn new() -> Self {
        Self::from_buffer([0; N], 0)
    }

    pub fn from_buffer(buffer: [u8; N], len: usize) -> Self {
        Self { buffer, len }
    }

    pub fn from_slice(data: &[u8]) -> Self {
        assert!(
            data.len() <= N,
            "Attempting to create a FastString with {} bytes which is more than the capacity ({N})",
            data.len()
        );
        let mut buffer = [0u8; N];
        buffer[..data.len()].copy_from_slice(data);
        Self { buffer, len: data.len() }
    }

    // this gives a warning that we should implement `std::str::FromStr::from_str` instead,
    // however that returns a `Result<Self, _>` while we need just `Self`,
    // as we assume that `data` is already a valid UTF-8 string.
    #[allow(clippy::should_implement_trait)]
    /// Converts a string slice to a `FastString`. Panics if there isn't enough capacity to store the data.
    /// If that is undesired, use `try_into` or `try_from` instead.
    pub fn from_str(data: &str) -> Self {
        Self::from_slice(data.as_bytes())
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    pub const fn capacity() -> usize {
        N
    }

    #[inline]
    pub fn push(&mut self, c: u8) {
        if self.len < N {
            self.buffer[self.len] = c;
            self.len += 1;
        } else {
            panic!("FastString buffer overflow (writing beyond capacity of {N})");
        }
    }

    pub fn extend(&mut self, data: &str) {
        for char in data.as_bytes() {
            self.push(*char);
        }
    }

    /// like `extend` but will simply truncate the data instead of panicking if the string doesn't fit
    pub fn extend_safe(&mut self, data: &str) {
        for char in data.as_bytes() {
            if self.len >= N {
                break;
            }

            self.push(*char);
        }
    }

    pub fn to_string(&self) -> Result<String> {
        Ok(self.to_str()?.to_owned())
    }

    pub fn to_str(&self) -> Result<&str> {
        std::str::from_utf8(&self.buffer[..self.len]).map_err(Into::into)
    }
}

impl<const N: usize> Encodable for FastString<N> {
    fn encode(&self, buf: &mut bytebuffer::ByteBuffer) {
        buf.write_u32(self.len as u32);
        buf.write_bytes(&self.buffer[..self.len]);
    }

    fn encode_fast(&self, buf: &mut FastByteBuffer) {
        buf.write_u32(self.len as u32);
        buf.write_bytes(&self.buffer[..self.len]);
    }
}

impl<const N: usize> EncodableWithKnownSize for FastString<N> {
    const ENCODED_SIZE: usize = size_of_types!(u32) + N;
}

impl<const N: usize> Decodable for FastString<N> {
    fn decode(buf: &mut ByteBuffer) -> Result<Self>
    where
        Self: Sized,
    {
        Self::decode_from_reader(&mut ByteReader::from_bytes(buf.as_bytes()))
    }

    fn decode_from_reader(buf: &mut ByteReader) -> Result<Self>
    where
        Self: Sized,
    {
        let len = buf.read_u32()? as usize;
        if len > N {
            bail!("string is too long ({len} chars) to fit into a FastString with capacity {N}");
        }

        let mut buffer = [0u8; N];
        std::io::Read::read(buf, &mut buffer[..len])?;

        Ok(Self::from_buffer(buffer, len))
    }
}

impl<const N: usize> fmt::Display for FastString<N> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.to_str().unwrap_or("<invalid UTF-8 string>"))
    }
}

impl<const N: usize> TryInto<String> for FastString<N> {
    type Error = anyhow::Error;
    fn try_into(self) -> anyhow::Result<String> {
        self.to_string()
    }
}

impl<const N: usize> TryFrom<String> for FastString<N> {
    type Error = anyhow::Error;
    fn try_from(value: String) -> anyhow::Result<Self> {
        TryFrom::<&str>::try_from(&value)
    }
}

impl<const N: usize> TryFrom<&str> for FastString<N> {
    type Error = anyhow::Error;
    fn try_from(value: &str) -> anyhow::Result<Self> {
        if value.len() > N {
            bail!(
                "Attempting to convert a string slice with size {} into a FastString with capacity {N}",
                value.len()
            );
        }

        Ok(Self::from_str(value))
    }
}

impl<const N: usize> Default for FastString<N> {
    fn default() -> Self {
        Self::new()
    }
}
