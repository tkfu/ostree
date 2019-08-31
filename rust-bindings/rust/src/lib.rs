//! # Rust bindings for **libostree**
//!
//! [libostree](https://ostree.readthedocs.io) is both a shared library and suite of command line
//! tools that combines a "git-like" model for committing and downloading bootable filesystem trees,
//! along with a layer for deploying them and managing the bootloader configuration.

#![doc(html_root_url = "https://fkrull.gitlab.io/ostree-rs")]

extern crate gio_sys;
extern crate glib_sys;
extern crate gobject_sys;
extern crate ostree_sys;
#[macro_use]
extern crate glib;
extern crate gio;
extern crate libc;
#[macro_use]
extern crate bitflags;
#[macro_use]
extern crate lazy_static;
#[cfg(feature = "futures")]
extern crate fragile;
#[cfg(feature = "futures")]
extern crate futures;

use glib::Error;

// code generated by gir
#[rustfmt::skip]
#[allow(clippy::all)]
#[allow(unused_imports)]
mod auto;
pub use crate::auto::functions::*;
pub use crate::auto::*;

// handwritten code
mod checksum;
#[cfg(any(feature = "v2018_6", feature = "dox"))]
mod collection_ref;
#[cfg(any(feature = "v2019_3", feature = "dox"))]
mod kernel_args;
mod object_name;
mod repo;
#[cfg(any(feature = "v2018_2", feature = "dox"))]
mod repo_checkout_at_options;
pub use crate::checksum::*;
#[cfg(any(feature = "v2018_6", feature = "dox"))]
pub use crate::collection_ref::*;
#[cfg(any(feature = "v2019_3", feature = "dox"))]
pub use crate::kernel_args::*;
pub use crate::object_name::*;
pub use crate::repo::*;
#[cfg(any(feature = "v2018_2", feature = "dox"))]
pub use crate::repo_checkout_at_options::*;

// tests
#[cfg(test)]
mod tests;

// prelude
pub mod prelude {
    pub use crate::auto::traits::*;
}
