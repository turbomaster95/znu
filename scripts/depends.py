#!/usr/bin/env python3
# depends.py — conditional-aware recursive C/C++ dependency tree

import argparse
import re
from pathlib import Path


class DependencyParser:
    def __init__(self, args):
        self.args = args
        self.include_dirs = [Path(d).resolve() for d in args.include]
        self.quote_dirs = [Path(d).resolve() for d in args.quote]
        self.macros = dict(m.split("=", 1) if "=" in m else (m, "1") for m in args.define)
        self.visited_once = set()
        self.active = []
        self.warnings = []

    def warn(self, msg):
        self.warnings.append(msg)
        if not self.args.quiet:
            print(f"warning: {msg}")

    def clean(self, text):
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        text = re.sub(r"//.*", "", text)
        return text.replace("\\\n", "").replace("\\\r\n", "")

    def expand(self, text):
        for _ in range(20):
            new = re.sub(r"\b[A-Za-z_]\w*\b", lambda m: str(self.macros.get(m.group(0), m.group(0))), text)
            if new == text:
                break
            text = new
        return text

    def resolve(self, name, current_dir, quoted=True):
        paths = ([current_dir] if quoted else []) + self.quote_dirs + self.include_dirs
        for directory in paths:
            candidate = directory / name
            if candidate.is_file():
                return candidate.resolve()
        return None

    def read_lines(self, path):
        try:
            return self.clean(Path(path).read_text(errors="replace")).splitlines()
        except OSError as e:
            self.warn(f"cannot read {path}: {e}")
            return []

    def parse_file(self, path, depth=0):
        path = Path(path).resolve()
        key = str(path)

        if depth > self.args.max_depth:
            return {"type": "file", "name": path.name, "status": "max depth reached", "children": []}
        if key in self.visited_once:
            return {"type": "file", "name": path.name, "status": "pragma once skipped", "children": []}
        if key in self.active:
            return {"type": "file", "name": path.name, "status": "circular include", "children": []}

        self.active.append(key)
        lines = self.read_lines(path)
        nodes, _, pragma_once = self.parse_region(lines, 0, path.parent, depth)
        if pragma_once:
            self.visited_once.add(key)
        self.active.pop()
        return {"type": "file", "name": path.name, "path": key, "children": nodes}

    def parse_region(self, lines, pos, current_dir, depth, stops=("elif", "else", "endif")):
        nodes = []
        pragma_once = False

        while pos < len(lines):
            line = lines[pos].strip()
            match = re.match(r"^#\s*(\w+)\s*(.*?)\s*$", line)
            if not match:
                pos += 1
                continue

            directive, arg = match.groups()

            if directive in stops:
                return nodes, pos, pragma_once

            if directive in ("if", "ifdef", "ifndef"):
                opener = f"#if {arg}" if directive == "if" else f"#{directive} {arg}"
                branches = []

                original_macros = self.macros.copy()
                branch_nodes, pos, branch_once = self.parse_region(lines, pos + 1, current_dir, depth, ("elif", "else", "endif"))
                pragma_once |= branch_once
                branches.append({"condition": opener, "children": branch_nodes})

                self.macros = original_macros.copy()

                while pos < len(lines):
                    next_match = re.match(r"^\s*#\s*(\w+)\s*(.*?)\s*$", lines[pos])
                    if not next_match:
                        pos += 1
                        continue

                    next_directive, next_arg = next_match.groups()

                    if next_directive == "elif":
                        branch_condition = f"#elif {next_arg}"
                        branch_nodes, pos, branch_once = self.parse_region(lines, pos + 1, current_dir, depth, ("elif", "else", "endif"))
                        pragma_once |= branch_once
                        branches.append({"condition": branch_condition, "children": branch_nodes})
                        self.macros = original_macros.copy()
                        continue

                    if next_directive == "else":
                        branch_nodes, pos, branch_once = self.parse_region(lines, pos + 1, current_dir, depth, ("endif",))
                        pragma_once |= branch_once
                        branches.append({"condition": "#else", "children": branch_nodes})
                        self.macros = original_macros.copy()
                        if pos < len(lines) and re.match(r"^\s*#\s*endif\b", lines[pos]):
                            pos += 1
                        break

                    if next_directive == "endif":
                        pos += 1
                        self.macros = original_macros.copy()
                        break

                    self.warn(f"{current_dir}: unexpected directive #{next_directive}")
                    pos += 1

                nodes.append({"type": "conditional", "branches": branches})
                continue

            if directive == "include":
                include_arg = self.expand(arg)
                include_match = re.match(r'^\s*"([^"]+)"', include_arg)
                quoted = True
                if not include_match:
                    include_match = re.match(r"^\s*<([^>]+)>", include_arg)
                    quoted = False

                if include_match:
                    name = include_match.group(1)
                    resolved = self.resolve(name, current_dir, quoted)
                    if resolved:
                        nodes.append(self.parse_file(resolved, depth + 1))
                    else:
                        nodes.append({"type": "file", "name": name, "status": "external/not found", "children": []})
                else:
                    nodes.append({"type": "file", "name": arg, "status": "unresolved include macro", "children": []})
                pos += 1
                continue

            if directive == "pragma" and arg == "once":
                pragma_once = True
                pos += 1
                continue

            if directive == "define":
                macro = re.match(r"^([A-Za-z_]\w*)(\s*)(.*)$", arg)
                if macro:
                    name, spacing, value = macro.groups()
                    if not (not spacing and value.startswith("(")):
                        self.macros[name] = value.strip() or "1"
                if self.args.show_directives:
                    nodes.append({"type": "directive", "text": f"#define {arg}"})
                pos += 1
                continue

            if directive == "undef":
                self.macros.pop(arg.split()[0] if arg else "", None)
                if self.args.show_directives:
                    nodes.append({"type": "directive", "text": f"#undef {arg}"})
                pos += 1
                continue

            if self.args.show_directives:
                nodes.append({"type": "directive", "text": f"#{directive} {arg}".rstrip()})

            pos += 1

        return nodes, pos, pragma_once

    def print_tree(self, node, prefix="", last=True):
        connector = "└── " if last else "├── "
        name = node.get("name", "?")
        status = node.get("status")
        if status:
            name += f" [{status}]"
        print(prefix + connector + name)

        children = node.get("children", [])
        child_prefix = prefix + ("    " if last else "│   ")
        for i, child in enumerate(children):
            self.print_node(child, child_prefix, i == len(children) - 1)

    def print_node(self, node, prefix="", last=True):
        if node["type"] == "conditional":
            branches = node["branches"]
            opener = branches[0]["condition"]
            connector = "└── " if last else "├── "
            print(prefix + connector + opener)
            branch_prefix = prefix + ("    " if last else "│   ")

            first_children = branches[0]["children"]
            alternatives = branches[1:]

            for i, child in enumerate(first_children):
                is_last = i == len(first_children) - 1 and not alternatives
                self.print_node(child, branch_prefix, is_last)

            for i, branch in enumerate(alternatives):
                branch_last = i == len(alternatives) - 1
                branch_connector = "└── " if branch_last else "├── "
                print(branch_prefix + branch_connector + branch["condition"])
                alt_prefix = branch_prefix + ("    " if branch_last else "│   ")
                for j, child in enumerate(branch["children"]):
                    self.print_node(child, alt_prefix, j == len(branch["children"]) - 1)

            return

        if node["type"] == "directive":
            connector = "└── " if last else "├── "
            print(prefix + connector + node["text"])
            return

        self.print_tree(node, prefix, last)


def main():
    ap = argparse.ArgumentParser(description="Print recursive C/C++ dependency tree with conditional branches")
    ap.add_argument("target", help="source or header file")
    ap.add_argument("-I", dest="include", action="append", default=[], metavar="DIR")
    ap.add_argument("-iquote", dest="quote", action="append", default=[], metavar="DIR")
    ap.add_argument("-D", dest="define", action="append", default=[], metavar="MACRO[=VALUE]")
    ap.add_argument("--show-directives", action="store_true", help="also display non-include preprocessor directives")
    ap.add_argument("--max-depth", type=int, default=128)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    target = Path(args.target).resolve()
    if not target.is_file():
        ap.error(f"file not found: {target}")

    parser = DependencyParser(args)
    tree = parser.parse_file(target)
    print(target.name)
    for i, child in enumerate(tree["children"]):
        parser.print_node(child, "", i == len(tree["children"]) - 1)


if __name__ == "__main__":
    main()
