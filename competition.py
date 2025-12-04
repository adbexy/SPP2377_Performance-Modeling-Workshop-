#!/usr/bin/env python3

import os
from glob import glob
from dataclasses import dataclass

glob_file_name = "my_results"
directory_patterns = [
    "/home/*/*",
    "/home/*",
]
def get_user(file_name):
    # assumes a /home/<user>/... file_name pattern
    return file_name.split("/")[2]

def get_file_names():
    return [ # flatten the files from all the directory_patterns
        file_name
        for directory_pattern in directory_patterns
        for file_name in glob(
            f"{directory_pattern}/{glob_file_name}",
            recursive = True,
        )
    ]

@dataclass
class Results:
    user: str
    fast_result: int
    safe_result: int
    inner_throughput: float
    outer_throughput: float

    def from_file_name(file_name):
        user = get_user(file_name)
        with open(file_name, "r") as file:
            lines = filter(len, file.read().split("\n"))
            try:
                fast_result, safe_result, inner_throughput, outer_throughput = lines
                return Results(
                    user,
                    int(fast_result),
                    int(safe_result),
                    float(inner_throughput),
                    float(outer_throughput),
                )
            except Exception as e:
                print(f"could not read results file\n  {file_name!r}\n  {e!r}")
                return Results(user, 0, 0, 0, 0)

    def format(value, format):
        Gibi = format.endswith("G")
        if Gibi:
            value /= 1024 ** 3
            format = format[:-1]

        result = value.__format__(format)
        if Gibi:
            result += " GiB/s"
        return result

    def __iter__(self):
        yield self.user
        yield self.fast_result
        yield self.safe_result
        yield self.inner_throughput
        yield self.outer_throughput

    def __format__(self, format_code):
        values = tuple(self)
        formats = format_code.split(",")
        assert len(values) == len(formats)

        return "".join(
            Results.format(value, format)
            for value, format in zip(values, formats)
            if format
        )

    def correct(self):
        return self.fast_result == self.safe_result

def get_table(file_names):
    return [
        Results.from_file_name(file_name)
        for file_name in file_names
    ]

def main():
    table = get_table(get_file_names())
    check = " ok "
    result = Results("user", "fast", "safe", "throughput", "")
    print(    f"{check}{result:20,,,>14,}")
    sorted_table = sorted(
        table,
        key = lambda result: result.outer_throughput,
        reverse = True,
    )
    for result in sorted_table:
        check = " :) " if result.correct() else " :( "
        print(f"{check}{result:20,,, 8.3fG,}")

if __name__ == "__main__":
    main()

