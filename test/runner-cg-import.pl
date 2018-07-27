#!/usr/bin/perl -w

$total = 0;
$passed = 0;
$max_regs = 0;

if (@ARGV) {
	@inputs = @ARGV;
} else {
	@inputs = glob("input/*.c");
}

foreach $max_regs (4..8) {
foreach $input (@inputs) {

	print "\nTest is $input (--cg-max-regs=$max_regs)\n";
	$total = $total + 1;

	print "  Compiling reference assembly...";
	if (0 == system("../build/driver $input --dump-cg --cg-max-regs=$max_regs > /dev/null 2> /dev/null")) {
		print "success\n";
	} else {
		print "failed\n";
		next;
	}

	print "  Importing codegen (cg_00_iselect)...";
	if (0 == system("../build/driver --cg-import=cg_00_iselect.txt --cg-max-regs=$max_regs --cg-run-ra --cg-run-branch-predication --cg-run-emit=cg_00_iselect.s > /dev/null 2> /dev/null")) {
		print "success\n";
	} else {
		print "failed\n";
		next;
	}

	print "  Comparing assembly...";
	if (0 == system("diff $input.s cg_00_iselect.s")) {
		print "success\n";
	} else {
		print "failed\n";
		next;
	}

	print "  Importing codegen (cg_01_regalloc)...";
	if (0 == system("../build/driver --cg-import=cg_01_regalloc.txt --cg-run-branch-predication --cg-run-emit=cg_01_regalloc.s > /dev/null 2> /dev/null")) {
		print "success\n";
	} else {
		print "failed\n";
		next;
	}

	print "  Comparing assembly...";
	if (0 == system("diff $input.s cg_01_regalloc.s")) {
		print "success\n";
	} else {
		print "failed\n";
		next;
	}

	print "  Importing codegen (cg_02_branch_predication)...";
	if (0 == system("../build/driver --cg-import=cg_02_branch_predication.txt --cg-run-emit=cg_02_branch_predication.s > /dev/null 2> /dev/null")) {
		print "success\n";
	} else {
		print "failed\n";
		next;
	}

	print "  Comparing assembly...";
	if (0 == system("diff $input.s cg_02_branch_predication.s")) {
		print "success\n";
	} else {
		print "failed\n";
		next;
	}

	$passed = $passed + 1;
}
}

print "\n---\nPassed ($passed/$total)\n";

