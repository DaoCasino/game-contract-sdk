#!/bin/python
from itertools import chain
from functools import reduce
from json import load as json_load, dump as json_dump
from pathlib import Path
from typing import Iterable, Optional


def deep_merge_list_of_dict(left: list, right: Optional[list]) -> list:
    """
    Merge dict-values in two lists, identify them by `%name%` key

    if values are not dict -> `AssertionError`
    if internal dicts haven't got any key with `%name%` -> `AssertionError`
    """
    if not right:
        return left

    if not left:
        return right

    assert all(isinstance(value, dict) for value in left)
    assert all(isinstance(value, dict) for value in right)

    keys = chain(*(tuple(attributes.keys()) for attributes in left))

    try:
        identity = next(filter(lambda key: 'name' in key, keys))
    except StopIteration:
        raise AssertionError(f"Can't find name in internal dict left: {left} right: {right}")

    left = {attributes[identity]: attributes for attributes in left}
    right = {attributes[identity]: attributes for attributes in right}

    return list(deep_merge_dict(left, right).values())


def deep_merge_dict(left: dict, right: Optional[dict]):
    """
    Deep merge dicts of dicts and lists with dicts by keys
    """
    if not right:
        return left

    if not left:
        return left

    for key, value in right.items():
        if isinstance(value, dict):
            left[key] = deep_merge_dict(value, left.get(key))
        elif isinstance(value, list):
            left[key] = deep_merge_list_of_dict(value, left.get(key))
        elif key in left and left[key] != value:
            choose = None
            while choose not in ['1', '2']:
                choose = input(f"Choose variant (1 or 2)\n 1. '{value}'\n 2. '{left[key]}'\n")
                if choose == '1':
                    left[key] = value
        else:
            left[key] = value

    return left


def merge_abi(abi_collection: Iterable[dict]) -> dict:
    return reduce(lambda sum, abi: deep_merge_dict(sum, abi), abi_collection)


def merge_abi_files(files: Iterable[Path], result_path: Path):
    files = (open(str(path), 'r') for path in files)

    json_dump(obj=merge_abi(map(json_load, files)),
              fp=open(str(result_path), 'w'),
              indent=True)

    [file.close() for file in files]


def main():
    import sys
    if len(sys.argv) < 3:
        print("Wrong call arguments\nExample: merge_abi.py input1.abi input2.abi ... result.abi")
        exit(-1)

    files = tuple(Path(fn) for fn in sys.argv[1:])
    files, output = files[:-1], files[-1]

    assert all(file.exists() for file in files)

    merge_abi_files(files=files, result_path=output )


if __name__ == "__main__":
    main()

