//! Snapshot-based undo/redo: each undoable mutation stores the full PPES
//! project buffer beforehand. Simple and covers every operation uniformly;
//! can be refined into per-op commands (PESUndoRedoCommand-style) later.

use crate::engine::{with_engine, Engine};
use std::sync::Mutex;

const MAX_DEPTH: usize = 50;

#[derive(Default)]
struct History {
    undo: Vec<Vec<u8>>,
    redo: Vec<Vec<u8>>,
}

static HISTORY: Mutex<History> = Mutex::new(History {
    undo: Vec::new(),
    redo: Vec::new(),
});

fn lock() -> std::sync::MutexGuard<'static, History> {
    HISTORY.lock().unwrap_or_else(|e| e.into_inner())
}

pub fn can_undo() -> bool {
    !lock().undo.is_empty()
}

pub fn can_redo() -> bool {
    !lock().redo.is_empty()
}

pub fn clear() {
    let mut h = lock();
    h.undo.clear();
    h.redo.clear();
}

fn snapshot(eng: &Engine) -> Vec<u8> {
    eng.export_as("PPES")
}

fn restore(eng: &Engine, buf: &[u8]) {
    eng.new_document();
    eng.load_ppes(buf);
}

/// Run a mutation with the pre-state pushed onto the undo stack.
pub fn run_undoable<R>(f: impl FnOnce(&Engine) -> R) -> R {
    with_engine(|eng| {
        let before = snapshot(eng);
        let result = f(eng);
        let mut h = lock();
        h.undo.push(before);
        if h.undo.len() > MAX_DEPTH {
            h.undo.remove(0);
        }
        h.redo.clear();
        result
    })
}

pub fn undo() -> bool {
    with_engine(|eng| {
        let mut h = lock();
        let Some(buf) = h.undo.pop() else {
            return false;
        };
        h.redo.push(snapshot(eng));
        drop(h);
        restore(eng, &buf);
        true
    })
}

pub fn redo() -> bool {
    with_engine(|eng| {
        let mut h = lock();
        let Some(buf) = h.redo.pop() else {
            return false;
        };
        h.undo.push(snapshot(eng));
        drop(h);
        restore(eng, &buf);
        true
    })
}
