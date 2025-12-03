#!/usr/bin/env python3

import os
from glob import glob
from dataclasses import dataclass

file_name = "my_results"
directory_pattern = "/home/*/*"
def get_user(file_name):
    # assumes a /home/<user>/... file_name pattern
    return file_name.split("/")[2]

def get_file_names():
    return glob(
        f"{directory_pattern}/{file_name}",
        recursive = True,
    )

@dataclass
class Results:
    fast_result: int
    safe_result: int
    inner_throughput: float
    outer_throughput: float

    def from_file(file):
        lines = filter(len, file.read().split("\n"))
        try:
            fast_result, safe_result, inner_throughput, outer_throughput = lines
            return Results(
                int(fast_result),
                int(safe_result),
                float(inner_throughput),
                float(outer_throughput),
            )
        except Exception as e:
            print("could not read one results file...")
            print(e.what())
            return Results(0, 0, 0, 0)

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
    result = {} # user: Results
    for file_name in file_names:
        user = get_user(file_name)
        with open(file_name, "r") as file:
            result[user] = Results.from_file(file)
    return result

def main():
    user = "user"
    check = " ok "
    result = Results("fast", "safe", "inner and", "outer throughput")
    print(    f"{user:20} {check}{result:,,14,14}")
    for user, result in get_table(get_file_names()).items():
        check = " :) " if result.correct() else " :( "
        print(f"{user:20} {check}{result:,, 8.3fG, 8.3fG}")

if __name__ == "__main__":
    main()

