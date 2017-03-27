import re, sys, io, os, bz2, threading
from datetime import datetime

term_freq = {}
term_title = {}

def getlinenum(infilename):
	linecount = 0
	fin = bz2.BZ2File(infilename, 'r')
	for line in fin:
		linecount += 1
	fin.close()
	return linecount

def get_anchor(infilename): #, start, end):
	fin = bz2.BZ2File(infilename, 'r')
	pattern = re.compile('\[\[[^\[^\]]*\]\]')
	linecount = 0
	for line in fin:
		'''if linecount < start:
			linecount += 1
			continue
		if linecount >= end:
			break'''
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
			hastitle = (len(names) > 1)
			if not hastitle:
				term = names[0]
			else:
				term = names[1]
			if len(term.split(' ')) < 2:
				continue
			if hastitle and term not in term_title:
				term_title[term] = names[0]
			if term not in term_freq:
				term_freq[term] = 1
			else:
				term_freq[term] += 1		
	fin.close()
	
def output_result(outfilename):
	fout = open(outfilename, 'w')
	for k, v in term_freq.iteritems():
		fout.write('\t'.join([k, str(v)]))
		if k in term_title:
			fout.write('\t')
			fout.write(term_title[k])
		fout.write('\n')
	fout.close()
			
if __name__=='__main__':
	print datetime.now().time()
	infilename = '/home/xwang95/data/wikipedia/enwiki-latest-pages-articles.xml.bz2' 
	outfilename = '/home/sjiang18/Data/plans/wiki.anchor'
	'''tnum = 50
	linenum = getlinenum(infilename)
	k = linenum / tnum
	for i in range(tnum):
		start = i * k
		end = min(linenum, (i+1)*k)
		t = threading.Thread(target = get_anchor, args = (infilename, start, end))
		t.start()
		t.join()
	'''
	get_anchor(infilename)
	print 'processing done: ', datetime.now().time()
	output_result(outfilename) 
	print 'output done: ', datetime.now().time()
