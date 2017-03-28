import re, sys, io, os, bz2, threading
from datetime import datetime

term_freq = {}

def getlinenum(infilename):
	linecount = 0
	fin = bz2.BZ2File(infilename, 'r')
	for line in fin:
		linecount += 1
	fin.close()
	return linecount

def get_anchor(infilename, start, end):
	fin = bz2.BZ2File(infilename, 'r')
	pattern = re.compile('\[\[[^\[^\]]*\]\]')
	linecount = 0
	for line in fin:
		if linecount < start:
			linecount += 1
			continue
		if end > 0 and linecount >= end:
			break
		linecount += 1
		if linecount % 1000000 == 0:
			sys.stdout.write("\r" + 'processing the ' + str(linecount) + '-th line')
			sys.stdout.flush()
		line = line.strip()
		if line.startswith('<') or line.startswith('{'):
			continue
		f = pattern.findall(line)
		for word in f:
			word = word[2:-2].strip()
			if word == '':
				continue
			names = word.split('|')
			for term in names:
				term = term.replace('\t', ' ').strip()
				term_freq[term] = term_freq.get(term, 0) + 1
	fin.close()
	
def output_result(outfilename):
	fout = open(outfilename, 'w')
	sort = sorted(term_freq.items(), key = lambda x:x[1], reverse= True)
	for k, v in sort:
		fout.write('\t'.join([k, str(v)]))
		fout.write('\n')
	fout.close()
			
if __name__=='__main__':
	print datetime.now().time()
	infilename = '/home/xwang95/data/wikipedia/enwiki-latest-pages-articles.xml.bz2' 
	outfilename = '/home/sjiang18/data/plans/wiki.anchor'
	tnum = 20
	print 'processing start: ', datetime.now().time()
	'''
	linenum = getlinenum(infilename)
	k = linenum / tnum
	for i in range(1):#(tnum):
		start = i * k
		end = min(linenum, (i+1)*k)
		t = threading.Thread(target = get_anchor, args = (infilename, start, end))
		t.start()
		t.join()
	'''
	get_anchor(infilename, 0, -1)
	print 'processing done: ', datetime.now().time()
	output_result(outfilename) 
	print 'output done: ', datetime.now().time()
