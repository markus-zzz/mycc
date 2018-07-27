#!/usr/bin/perl -w

$total = 0;
$passed = 0;
$max_regs = 0;

if ($ENV{'PREFIX'}) {
	$prefix = $ENV{'PREFIX'};
} else {
	$prefix = '';
}

if (@ARGV) {
	@inputs = @ARGV;
} else {
	@inputs = glob("input/*.c");
}

foreach $max_regs (4..8) {
foreach $input (@inputs) {
	$ref_result = undef;
	$target_result = undef;

	print "\nTest is $input (--cg-max-regs=$max_regs)\n";
	$total = $total + 1;

	print "  Compiling reference...";
	if (0 == system("gcc $input harness.c")) {
		print "success\n";
	} else {
		print "failed\n";
		next;
	}

	print "  Running reference...";
	if (open(OUT, "./a.out |") and @out = <OUT> and $out[-1] =~ m/RESULT:(0x[a-fA-F0-9]+)/) {
		$ref_result = $1;
		print "success [$ref_result]\n";
	} else {
		print "failed\n";
		next;
	}

	print "  Compiling target...";
	if (0 == system("../build/driver $input --sim-ir=run_test --cg-max-regs=$max_regs > /dev/null 2> /dev/null")) {
		print "success\n";
	} else {
		print "failed\n";
		next;
	}

	$all_sims_passed = 1;
	@sims = glob("sim_??*.txt");
	foreach $sim (@sims) {
		open(F, $sim);
		@lines = <F>;
		close(F);
		$lines[-1] =~ m/^ret[^[]*\[([^]]+)\]$/;
		$sim_result = $1;
		$sim =~ m/sim_([^.]+)\.txt/;
		print "  Simulating IR after $1...";
		if ($ref_result eq $sim_result) {
			print "success [$sim_result]\n";
		} else {
			print "failed [$sim_result]\n";
			$all_sims_passed = 0;
			last;
		}
	}
	if ($all_sims_passed == 0) {
		next;
	}

	print "  Checking register usage...";
	open(F, "$input.s");
	@assembly = <F>;
	close(F);
	if (!grep(/r[$max_regs-9]/, @assembly)) {
		print "success\n";
	} else {
		print "failed\n";
		next;
	}

	print "  Assembling target...";
	if (0 == system($prefix . "arm-linux-gnueabihf-gcc --static $input.s harness.c > /dev/null 2> /dev/null")) {
		print "success\n";
	} else {
		print "failed\n";
		next;
	}

	print "  Running target...";
	if (open(OUT, "./a.out |") and @out = <OUT> and $out[-1] =~ m/RESULT:(0x[a-fA-F0-9]+)/) {
		$target_result = $1;
		print "success [$target_result]\n";
	} else {
		print "failed\n";
		next;
	}

	print "  Comparing results...";
	if ($ref_result eq $target_result) {
		print "success\n";
		$passed = $passed + 1;
	} else {
		print "failed\n";
	}
}
}

print "\n---\nPassed ($passed/$total)\n";

