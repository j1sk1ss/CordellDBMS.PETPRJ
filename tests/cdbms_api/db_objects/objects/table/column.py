from __future__ import annotations
from enum import Enum


class ColumnDataType(Enum):
    INT = 'int'
    STR = 'str'
    ANY = 'any'
    FLT = 'dob'
    NONE = ''

    def module(self, body) -> ColumnDataType:
        self._dynamic_value = body
        return self

    @property
    def value(self) -> str:
        return getattr(self, '_dynamic_value', self._value_)


class ColumnType(Enum):
    PRIMARY = 'p'
    NOT_PRIMARY = 'np'
    AUTO_INCREMENT = 'a'
    WHITOUT_AUTO = 'na'


class Column:
    def __init__(self, name: str, data: ColumnDataType, type: list[ColumnType], size: int = 8) -> None:
        if len(name) > 8:
            print('Name of column too large! Should be 8 or less, but provided', len(name))
            return

        if size > 255:
            print('Column size too large. Should be 255 or less, but provided', size)
            return

        if len(type) != 2:
            print('Wrong type. Should be 2 elements, but provided', len(type))
            return

        self._name = name
        self._data = data
        self._type = type
        self._size = size

    @property
    def body(self) -> str:
        return f'{self._name} {self._size} "{self._data.value}" {self._type[0].value} {self._type[-1].value}'

    @property
    def name(self) -> str:
        return self._name

    @property
    def size(self) -> int:
        return self._size

    @property
    def data_type(self) -> ColumnDataType:
        return self._data
