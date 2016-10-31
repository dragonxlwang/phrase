import math, sys, io, os, threading, Queue
from datetime import datetime

word_freq = {}
term_freq = {}
keep = set()

def getlinenum (filename):
	fin = open(filename)
	linenum = sum([1 for line in fin]) 
	fin.close()
	return linenum

def get_wordfreq(filename, startline, endline):
	N = 0
	fin = open(filename)
	linenum = 0
	for line in fin:
		if linenum < startline:
			linenum += 1
			continue
		if endline != -1 and linenum >= endline:
			break	
		linenum += 1
		words_ori = line.split(' ')
		words = []
		for i in range(len(words_ori)):
			word = words_ori[i].lower().strip()
			if word == "" or word[0].isalpha() == False or len(word) > 20:
				continue
			words.append(word)
		length = len(words)
		for i in range(length):
			word = words[i]
			N += 1
			if word not in word_freq:
				word_freq[word] = 1
			else:
				word_freq[word] += 1
	fin.close()
	return N

def get_termfreq(filename, startline, endline):
	M = 0
	fin = open(filename)
	linenum = 0
	for line in fin:
		if linenum < startline:
			linenum += 1
			continue
		if linenum >= endline:
			break	
		linenum += 1
		words = line.split(' ')
		keeps = []
		for i in range(len(words)):
			words[i] = words[i].lower().strip()
			keeps.append((words[i] in keep))
		length = len(words)
		for i in range(length - 1):
			if not keeps[i] or not keeps[i + 1]: 
				continue
			M += 1
			term = ' '.join([words[i], words[i+1]])
			if term not in term_freq:
				term_freq[term] = 1
			else:
				term_freq[term] += 1 
	fin.close()
	return M 

def output_result(filename, N, M):
	fout = open(filename, 'w')
	for term in term_freq:
		pab = float(term_freq[term]) / float(M)
		if pab <= 0:
			continue
		a, b = term.split(' ')
		if a not in word_freq or b not in word_freq:
			continue
		pa = float(word_freq[a]) / float(N)
		pb = float(word_freq[b]) / float(N)
		if pa <= 0 or pb <= 0:
			continue
		term_freq[term] = math.log(pab) - math.log(pa) - math.log(pb)

	sort = sorted(term_freq.items(), key = lambda x:x[1], reverse=True)
	for k, v in sort:
		fout.write('\t'.join([k, str(v)]))
		fout.write('\n')
	fout.close()

def filter_word(K):
	sort = sorted(word_freq.items(), key = lambda x:x[1], reverse = True)
	for k, v in sort:
		keep.add(k)

def process_file(infilename, tum, K):
	#infilename = '/home/xwang95/data/gigaword/giga_nyt.txt'
	#outfilename = '/home/sjiang18/Data/plans/pmi.nyt'
	linenum = getlinenum(infilename)
	print 'number of lines in file: ', linenum, datetime.now().time()
	k = linenum / tnum
	que = Queue.Queue()
	for i in range(tnum):
		startline = i * k
		endline = min (linenum, (i + 1) * k)
		t = threading.Thread(target = lambda q, (arg1, arg2, arg3): q.put(get_wordfreq(arg1, arg2, arg3)), args = (que, (infilename, startline, endline)))
		t.start()
		t.join()
	print 'Got word freq ', datetime.now().time()
	N = 0
	while not que.empty():
		N += que.get() 
	filter_word(K)

	que = Queue.Queue()
	for i in range(tnum):
		startline = i * k
		endline = min (linenum, (i + 1) * k)
		t = threading.Thread(target = lambda q, (arg1, arg2, arg3): q.put(get_termfreq(arg1, arg2, arg3)), args = (que, (infilename, startline, endline)))
		t.start()
		t.join()
	print 'Got term freq ', datetime.now().time()
	M = 0
	while not que.empty():
		M += que.get()
	return N, M
	
if __name__=='__main__':
	#indirname = sys.args[1]
	#outfilename = sys.args[2]
	print datetime.now().time()
	indirname = '/home/xwang95/data/gigaword/giga_nyt.txt'
	outfilename = '/home/sjiang18/Data/plans/pmi.nyt'
	tnum = 50
	N = 0
	M = 0
	K = 100000
	N, M = process_file(indirname, tnum, K)
	output_result(outfilename, N, M)
	print datetime.now().time()
	print 'Got PMI ', datetime.now().time()
