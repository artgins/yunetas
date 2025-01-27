#!/usr/bin/env python3

import os
import argparse

# List of default substitutions (original, new)
DEFAULT_SUBSTITUTIONS = [
    ("ASN_OCTET_STR,",      "DTP_STRING,   "),
    ("ASN_UNSIGNED,",       "DTP_INTEGER, "),
    ("ASN_BOOLEAN",         "DTP_BOOLEAN"),
    ("ASN_COUNTER64,",      "DTP_INTEGER,  "),
    ("ASN_INTEGER",         "DTP_INTEGER"),
    ("ASN_UNSIGNED64,",     "DTP_INTEGER,   "),
    ("ASN_JSON",            "DTP_JSON"),
    ("ASN_POINTER",         "DTP_POINTER"),
    ("ASN_SCHEMA",          "DTP_SCHEMA"),
    ("GCLASS_TIMER",        "C_TIMER"),
    ("log_error(",          "gobj_log_error(gobj, "),
    ("log_info(",           "gobj_log_info(gobj, "),
    ("log_warning(",        "gobj_log_warning(gobj, "),

]

def substitute_strings_in_file(filename, substitutions):
    """
    Substitutes strings in a file based on the substitutions list.

    Args:
        filename (str): Path to the file to modify.
        substitutions (list of tuples): List of (original_string, new_string) pairs.
    """
    if not os.path.isfile(filename):
        print(f"Error: File '{filename}' does not exist.")
        return

    try:
        # Read the content of the file
        with open(filename, 'r', encoding='utf-8') as file:
            content = file.read()

        # Perform the substitutions
        for original, new in substitutions:
            content = content.replace(original, new)

        # Write the updated content back to the file
        with open(filename, 'w', encoding='utf-8') as file:
            file.write(content)

        print(f"Substitutions completed successfully for file: {filename}")

    except FileNotFoundError:
        print("Error: The file could not be found.")
    except PermissionError:
        print("Error: Permission denied. Cannot modify the file.")
    except Exception:
        print("Error: An unexpected error occurred while processing the file.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Substitute strings in a file based on a predefined list of substitutions.")
    parser.add_argument("filename", type=str, help="The path to the file to modify.")

    args = parser.parse_args()

    # Perform the substitutions using the default list
    substitute_strings_in_file(args.filename, DEFAULT_SUBSTITUTIONS)

