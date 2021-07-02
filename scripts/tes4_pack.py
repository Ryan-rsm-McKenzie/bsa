import argparse, enum, itertools, os, subprocess

@enum.unique
class Flags(enum.IntFlag):
	directory_strings = 1 << 0
	file_strings = 1 << 1
	compressed = 1 << 2
	embedded_file_names = 1 << 8

def powerset(a_iterable):
	s = list(a_iterable)
	return itertools.chain.from_iterable(
		itertools.combinations(s, r) for r in range(len(s)+1))

def combinate(a_args):
	combinations = powerset([flag.value for flag in Flags])
	formats = ( "tes4", "tes5", "sse" )
	for format in formats:
		for combo in combinations:
			af = list(itertools.accumulate(combo, lambda sum, elem: sum | elem, initial=0))[-1]
			subprocess.run([
				a_args.bsarch,
				"pack",
				a_args.directory,
				"{}_{:X}.bsa".format(format, af),
				"-{}".format(format),
				"-af:0x{:X}".format(af),
			],
			check=True)

def parse_arguments():
	parser = argparse.ArgumentParser(description="use the input directory to generate archives with different flags/formats")
	parser.add_argument("bsarch", type=str, help="the full path to bsarch")
	parser.add_argument("directory", type=str, help="the full path to the directory to archive")
	return parser.parse_args()

def main():
	args = parse_arguments()

	os.chdir(os.path.dirname(os.path.realpath(__file__)))
	out = "bin"
	os.makedirs(out, exist_ok=True)
	os.chdir(out)

	combinate(args)

if __name__ == "__main__":
	main()
