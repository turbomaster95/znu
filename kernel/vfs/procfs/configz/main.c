// @proccom:device
// name: config.gz
// type: VFS_FILE
// ops: procfs_config_ops

asm (
"	.pushsection .rodata, \"a\"		\n"
"	.ascii \"CFG_ST\"			\n"
"	.global kernel_config_data		\n"
"kernel_config_data:				\n"
"	.incbin \"kernel/configd.gz\"   	\n"
"	.global kernel_config_data_end		\n"
"kernel_config_data_end:			\n"
"	.ascii \"CFG_ED\"			\n"
"	.popsection				\n"
);

extern char kernel_config_data;
extern char kernel_config_data_end;

static ssize_t kconfig_read_current(char *buf, size_t len, loff_t * offset) {
	return simple_read_from_buffer(buf, len, offset,
				       &kernel_config_data,
				       &kernel_config_data_end -
				       &kernel_config_data);
}

static const struct proc_ops config_gz_proc_ops = {
	.proc_read	= kconfig_read_current,
	.proc_lseek	= default_llseek,
};

static int kconfig_init(void) {
	struct proc_dir_entry *entry;

	/* create the current config file */
	entry = proc_create("config.gz", S_IFREG | S_IRUGO, NULL,
			    &config_gz_proc_ops);
	if (!entry)
		return -ENOMEM;

	proc_set_size(entry, &kernel_config_data_end - &kernel_config_data);

	return 0;
}

static void kconfig_cleanup(void) {
	remove_proc_entry("config.gz", NULL);
}
