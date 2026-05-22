import re
import os
import sys
import fileinput
import platform

file_pattern = "[`'](.+)['']."
start_target_pattern = " *Considering target file " + file_pattern
pruning_pattern = " *Pruning file " + file_pattern
end_target_pattern = " *Successfully remade target file " + file_pattern
no_remake_pattern = " *No need to remake target " + file_pattern
no_remake_vpath_pattern = " *No need to remake target " + file_pattern + "; using VPATH name " + file_pattern
command_pattern = " *Must remake target " + file_pattern

cmd_builtins = ("CD", "CHDIR", "COPY", "DEL", "ERASE", "DIR", "MD", "MKDIR", "PATH",
                "REN", "RENAME", "RD", "RMDIR", "CLS", "ECHO", "EXIT", "TYPE", "DATE",
                "TIME", "BREAK", "CALL", "CHCP", "FOR", "GOTO", "IF", "PAUSE", "PROMPT",
                "REM", "SET", "SHIFT", "VER", "VERIFY", "VOL")

def is_valid_command(cmd_line):
    cmd = cmd_line.strip()
    if not cmd:
        return False
        
    ignore_keywords = [
        "update target", "due to:", "Putting child", "Live child", "Reaping winning",
        "Removing child", "GNU Make", "Copyright (C)", "License GPL", "This is free software",
        "There is NO WARRANTY", "Reading makefile", "Updating makefiles", "search path",
        "Makefile:", "KBUILD_", "recursively expanding"
    ]
    if any(kw in cmd for kw in ignore_keywords):
        return False
        
    if cmd.startswith("make ") or " make " in cmd or cmd.startswith("+make"):
        return False

    valid_starts = (
        "gcc", "clang", "x86_64", "ld", "ar", "as", "set", "echo", "cat", "mv", 
        "rm", "mkdir", "cp", "touch", "sed", "awk", "cmd_", "@", "/", "objcopy",
        "nm", "scripts/", "python", "perl", "bash", "@("
    )
    if cmd.startswith(valid_starts) or "fobj=" in cmd or "cmd_" in cmd or "cpio" in cmd:
        return True
        
    return False

def from_lines(lines):
    target_stack = []
    target_map = {}
    target_commands = {}
    target_sorted = []
    target_path = {}

    while len(lines) > 0:
        line = lines.pop()

        # Add target
        match = re.search(start_target_pattern, line)
        if match != None:
            target_name = match.group(1)
            if len(target_stack) > 0:
                target_map[target_stack[-1]].append(target_name)
            target_stack.append(target_name)
            if target_name not in target_map:
                target_map[target_name] = []
            if target_name not in target_commands:
                target_commands[target_name] = []
            target_sorted.append(target_name)
            continue
        
        # Pruning file
        match = re.search(pruning_pattern, line)
        if match != None:
            if len(target_stack) > 0:
                target_name = target_stack[-1]
                dep = match.group(1)
                if target_name != dep:
                    target_map[target_name].append(dep)
            continue

        # Remove target, built
        match = re.search(end_target_pattern, line)
        if match != None:
            if target_stack:
                target_stack.pop()
            continue

        # Remove target, no need to build, with VPATH
        match = re.search(no_remake_vpath_pattern, line)
        if match != None:
            if target_stack:
                target_stack.pop()
            target_name = match.group(1)
            path = match.group(2)
            target_path[target_name] = path
            continue

        # Remove target, no need to build
        match = re.search(no_remake_pattern, line)
        if match != None:
            if target_stack:
                target_stack.pop()
            continue

        # Rebuild target, with command following
        match = re.search(command_pattern, line)
        if match != None:
            if len(target_stack) > 0:
                target_name = target_stack[-1]
                while len(lines) > 0:
                    next_line = lines[-1]
                    if next_line.startswith(" ") or "Successfully" in next_line or "Considering" in next_line or "Must remake" in next_line:
                        break
                    candidate_line = lines.pop()
                    if is_valid_command(candidate_line):
                        target_commands[target_name].append(candidate_line)
            continue

    return (target_map, target_commands, target_sorted, target_path)

if __name__ == "__main__":
    lines = []
    for line in fileinput.input():
        lines.append(line)
    lines.reverse()

    (target_map, target_commands, target_sorted, target_path) = from_lines(lines)
    target_sorted.reverse()

    output = []
    seen_rules = set()
    seen_targets = set()
    all_dependencies = set()
    
    PROJECT_BASE = "/home/deva/Projects/znu/"
    RAMFS_PATH = "configs/iso_root/boot/initramfs.cpio"

    # Regex to strip ANSI colors
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    
    # Regex to find any echo/printf command:
    strip_pattern = re.compile(r'(@?)(set\s+-e;?\s*)?(echo|printf)\s+([-e\s]*)(["\'])(.*?)\5', re.IGNORECASE)

    for target_name in target_sorted:
        if "FORCE" in target_name or target_name.endswith(".d"):
            continue

        commands = target_commands.get(target_name, [])
        if len(commands) == 0:
            continue

        clean_target = target_name.replace(PROJECT_BASE, "").strip()
        if not clean_target or clean_target in seen_targets:
            continue
        seen_targets.add(clean_target)

        rule_id = clean_target.replace("/", "_").replace(".", "_").replace("-", "_").replace("+", "_")
        if rule_id in seen_rules:
            rule_id += f"_{len(seen_rules)}"
        seen_rules.add(rule_id)

        desc_string = f"Building {clean_target}"
        filtered_commands = []
        
        for cmd in commands:
            match = strip_pattern.search(cmd)
            if match:
                raw_text = ansi_escape.sub('', match.group(6))
                desc_string = " ".join(raw_text.split())
            
            clean_cmd = strip_pattern.sub('', cmd)
            parts = re.split(r'&&|;', clean_cmd)
            new_parts = []
            for p in parts:
                p = p.strip()
                if not p: continue
                # Discard kbuild artifacts that redirect output to .cmd files
                if p.startswith(">"): continue
                
                # Silence shell scripts
                if ".sh" in p and "> /dev/null" not in p:
                    p += " > /dev/null 2>&1"
                
                new_parts.append(p)
            
            if new_parts:
                filtered_commands.append(" && ".join(new_parts))

        if not filtered_commands:
            continue

        command = " && ".join(filtered_commands)
        command = command.replace('\n', '')
        command = command.replace('$', '$$')

        if 'Windows' == platform.system():
            if len(filtered_commands) > 1 or command.split(maxsplit=1)[0].upper() in cmd_builtins:
                command = 'cmd /c "' + command + '"'

        rule_block = f"rule {rule_id}_rule\n  command = {command}\n  description = {desc_string}"

        deps = target_map.get(target_name, [])
        dep_set = set()
        
        implicit_o_deps = re.findall(r'([\w\-./]+\.o\b)', command.replace('$$', '$'))
        for o_dep in implicit_o_deps:
            clean_o_dep = o_dep.replace(PROJECT_BASE, "").strip()
            if clean_o_dep and clean_o_dep != clean_target:
                dep_set.add(clean_o_dep.replace(':', '$:'))
                all_dependencies.add(clean_o_dep)

        for dep in deps:
            if dep in target_path:
                dep = target_path[dep]
            
            dep = dep.replace(PROJECT_BASE, "").strip()
            if dep and dep != "FORCE" and dep != clean_target and not dep.endswith(".d"):
                dep_escaped = dep.replace(':', '$:')
                dep_set.add(dep_escaped)
                all_dependencies.add(dep)

        target_output = clean_target.replace(':', '$:')
        if "init/init.elf" in clean_target or "init.elf" in clean_target:
            target_output += f" {RAMFS_PATH}"
            seen_targets.add(RAMFS_PATH)

        if clean_target == "znus":
            dep_set.add(RAMFS_PATH)

        dep_list = " ".join(dep_set)
        build_block = f"build {target_output}: {rule_id}_rule {dep_list}"

        output.append(rule_block)
        output.append(build_block)

    for dep in all_dependencies:
        clean_dep = dep.replace(PROJECT_BASE, "").strip()
        if clean_dep not in seen_targets and not os.path.exists(os.path.join(PROJECT_BASE, clean_dep)):
            output.append(f"build {clean_dep.replace(':', '$:')}: phony")

    print("\n".join(output))
