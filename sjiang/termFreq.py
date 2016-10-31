import io, os, sys

term_freq = {}

def build_dict(filenames):
	for filename in filenames:
		fin = open(filename)
		for line in fin:
			term = line[:-1].split('\t')[0]
			if term not in term_freq:
				term_freq[term] = 0
		fin.close()

def get_freq(filename):
	fin = open(filename)
	for line in fin:
		words = line[:-1].split(' ')
		for i in range(len(words) - 1):

	fin.close()	

def output_result(filename, threshold):
	fout = open(filename, 'w')
	for k,v in term_freq.iteritems():
		if v < threshold:
			continue
		fout.write(k)
		fout.write('\n')
	fout.close()

