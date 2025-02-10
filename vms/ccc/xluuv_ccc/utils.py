import contextlib
import logging
from threading import Lock
from typing import Generator, Generic, TypeVar, Dict

T = TypeVar("T")


# Mutex for non-primitive types
class Mutex(Generic[T]):
    def __init__(self, value: T):
        self._lock = Lock()
        self._value = value

    @contextlib.contextmanager
    def lock(self) -> Generator[T, None, None]:
        self._lock.acquire()
        try:
            yield self._value
        finally:
            self._lock.release()


def setup_logging(log_level: int):
    logging.basicConfig(
        level=log_level,
        format="%(asctime)s %(levelname)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )


def flatten_dict(i_dict: Dict, parent="") -> Dict:
    r_dict = {}
    for key, value in i_dict.items():
        extended_key = key
        if len(parent) > 0:
            extended_key = parent + "_" + key
        if isinstance(value, dict):
            r_dict |= flatten_dict(value, extended_key)
        else:
            r_dict[extended_key] = value
    return r_dict
