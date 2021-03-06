import io, os, sys, prefixTree
from datetime import datetime

wiki_freq = {}
def buildPrefixTree(filename):
	tree = prefixTree.PrefixTree()
	fin = open(filename)
	termNum = 0
	for line in fin:
		term, freq = line[:-1].split('\t')
		tree.addTerm(term)
		wiki_freq[term] = freq
		termNum += 1
		if termNum % 100000 == 0:
			sys.stdout.write("\r" + 'adding the ' + str(termNum) + '-th term')
			sys.stdout.flush()
	fin.close()
	return tree

def get_freq(tree, filename):
	fin = open(filename)
	dic = {}
	linecount = 0
	for line in fin:
		linecount += 1
		if linecount % 10000 == 0:
			sys.stdout.write("\r" + 'processing the ' + str(linecount) + '-th line') 
			sys.stdout.flush()
		line = line.strip().lower()
		terms = tree.getTerm(line)
		for term in terms:
			if term not in dic:
				dic[term] = 1
			else:
				dic[term] += 1	
	fin.close()	
	return dic

def output_result(filename, dic):
	fout = open(filename, 'w')
	sort = sorted(dic.items(), key = lambda x:x[1], reverse = True)
	for k,v in sort:
		fout.write('\t'.join([k, str(v), wiki_freq[k]]))
		fout.write('\n')
	fout.close()

if __name__=='__main__':
	nytfile = '/home/xwang95/data/gigaword/giga_nyt.txt' 
	wikifile = '/home/sjiang18/data/plans/wiki.term'
	outfile = '/home/sjiang18/data/plans/wiki.nyt.freq'
	print 'construct prefix tree: ', datetime.now()
	tree = buildPrefixTree(wikifile)
	print '\nconstruction done: ', datetime.now()
	print '\nextract terms'
	dic = get_freq(tree, nytfile)
	print '\nextraction done: ', datetime.now()
	print '\noutput the result'
	output_result(outfile, dic)
	print 'output done: ', datetime.now()
