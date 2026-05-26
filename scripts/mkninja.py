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
    if not cmd: return False
    ignore_keywords = ["update target", "due to:", "Putting child", "Live child", "Reaping winning", "Removing child", "GNU Make", "Copyright (C)", "License GPL", "This is free software", "There is NO WARRANTY", "Reading makefile", "Updating makefiles", "search path", "Makefile:", "KBUILD_", "recursively expanding"]
    if any(kw in cmd for kw in ignore_keywords): return False
    if cmd.startswith("make ") or " make " in cmd or cmd.startswith("+make"): return False
    valid_starts = ("gcc", "clang", "x86_64", "ld", "ar", "as", "set", "echo", "cat", "mv", "rm", "mkdir", "cp", "touch", "sed", "awk", "cmd_", "@", "/", "objcopy", "nm", "scripts/", "python", "perl", "bash", "@(")
    return cmd.startswith(valid_starts) or "fobj=" in cmd or "cmd_" in cmd or "cpio" in cmd

def from_lines(lines):
    target_stack = []
    target_map = {}
    target_commands = {}
    target_sorted = []
    target_path = {}
    while len(lines) > 0:
        line = lines.pop()
        match = re.search(start_target_pattern, line)
        if match:
            target_name = match.group(1)
            if len(target_stack) > 0: target_map[target_stack[-1]].append(target_name)
            target_stack.append(target_name)
            if target_name not in target_map: target_map[target_name] = []
            if target_name not in target_commands: target_commands[target_name] = []
            target_sorted.append(target_name)
            continue
        match = re.search(pruning_pattern, line)
        if match:
            if len(target_stack) > 0:
                target_name = target_stack[-1]
                dep = match.group(1)
                if target_name != dep: target_map[target_name].append(dep)
            continue
        match = re.search(end_target_pattern, line) or re.search(no_remake_pattern, line)
        if match:
            if target_stack: target_stack.pop()
            continue
        match = re.search(command_pattern, line)
        if match and len(target_stack) > 0:
            target_name = target_stack[-1]
            while len(lines) > 0:
                next_line = lines[-1]
                if next_line.startswith(" ") or "Successfully" in next_line or "Considering" in next_line or "Must remake" in next_line: break
                candidate_line = lines.pop()
                if is_valid_command(candidate_line): target_commands[target_name].append(candidate_line)
    return (target_map, target_commands, target_sorted, target_path)

def get_deps_from_cmd(obj_path):
    cmd_file = obj_path.replace('.o', '.o.cmd')
    if not os.path.exists(cmd_file): return []
    deps = []
    try:
        with open(cmd_file, 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                if line.startswith("deps_"):
                    capture = True; continue
                if capture and (line.strip() == "" or line.startswith(obj_path.split('/')[-1].split('.')[0])): break
                dep = line.strip().replace(" \\", "").strip()
                if "$(" not in dep and dep: deps.append(dep)
    except: return []
    return deps

if __name__ == "__main__":
    lines = [line for line in fileinput.input()]; lines.reverse()
    (target_map, target_commands, target_sorted, target_path) = from_lines(lines)
    target_sorted.reverse()
    output, seen_rules, seen_targets, all_dependencies = [], set(), set(), set()
    PROJECT_BASE = "/home/deva/Projects/znu/"
    RAMFS_PATH = "configs/iso_root/boot/initramfs.cpio"

    for target_name in target_sorted:
        if "FORCE" in target_name or target_name.endswith(".d"): continue
        commands = target_commands.get(target_name, [])
        if not commands: continue
        clean_target = target_name.replace(PROJECT_BASE, "").strip()
        if not clean_target or clean_target in seen_targets: continue
        seen_targets.add(clean_target)
        rule_id = clean_target.replace("/", "_").replace(".", "_").replace("-", "_").replace("+", "_")
        if rule_id in seen_rules: rule_id += f"_{len(seen_rules)}"
        seen_rules.add(rule_id)
        
        filtered_commands = []
        for cmd in commands:
            clean_cmd = re.sub(r'(@?)(set\s+-e;?\s*)?(echo|printf)\s+([-e\s]*)(["\'])(.*?)\5', '', cmd, flags=re.IGNORECASE)
            parts = [p.strip() for p in re.split(r'&&|;', clean_cmd) if p.strip() and not p.strip().startswith(">")]
            if parts: filtered_commands.append(" && ".join(parts))
        
        command = " && ".join(filtered_commands).replace('\n', '').replace('$', '$$')
        command_with_dep = f"{command} && python3 scripts/cmd2d.py {target_name}.cmd {target_name} > {target_name}.d"
        
        # Linker Script Detection
        ld_script = ""
        match_ld = re.search(r'-T\s+([^\s]+)', command)
        if match_ld: ld_script = match_ld.group(1).replace(PROJECT_BASE, "")

        rule_block = f"rule {rule_id}_rule\n  command = {command_with_dep}\n  description = Building {clean_target}\n  depfile = {target_name}.d\n  deps = gcc"
        
        # Determine Dependencies
        implicit_o_deps = re.findall(r'([\w\-./]+\.o\b)', command.replace('$$', '$'))
        dep_set = {d.replace(PROJECT_BASE, "").strip() for d in implicit_o_deps if d and d != clean_target}
        if ld_script: dep_set.add(ld_script)
        
        if target_name.endswith("built-in.o"):
            src_deps = [d.replace(PROJECT_BASE, "").strip() for d in target_map.get(target_name, [])]
        else:
            c_file = clean_target.replace('.o', '.c')
            src_deps = [c_file] if os.path.exists(os.path.join(PROJECT_BASE, c_file)) else []
        
        final_deps = {d for d in (list(dep_set) + src_deps + get_deps_from_cmd(target_name)) 
                      if d and d != 'FORCE' and not d.startswith("scripts/") and not d.startswith(".") and not "/." in d}
        
        target_output = clean_target.replace(':', '$:')
        if "init/init.elf" in clean_target: target_output += f" {RAMFS_PATH}"
        
        build_block = f"build {target_output}: {rule_id}_rule {' '.join([d.replace(':', '$:') for d in final_deps])}"
        output.extend([rule_block, build_block])

    print("\n".join(output))
