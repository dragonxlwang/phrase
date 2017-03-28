import bz2, sys
from datetime import datetime

def get_title(infilename, outfilename):
	fin = bz2.BZ2File(infilename, 'r')
	fout = open(outfilename, 'w')
	linecount = 0
	for line in fin:
		if linecount % 1000000 == 0:
			sys.stdout.write("\r" + 'processing the ' + str(linecount) + '-th line')
			sys.stdout.flush()
		linecount += 1
		line = line.strip()
		if line.startswith('<title>') and line.endswith('</title>'):
			title = line[7:-8].strip()
			if len(title.split(' ')) < 2:
				continue
			fout.write(title)
			fout.write('\n')
	fin.close()
	fout.close()			
	print '\n'

if __name__=='__main__':
	print datetime.now().time() 
	infilename = '/home/xwang95/data/wikipedia/enwiki-latest-pages-articles.xml.bz2'
	outfilename = '/home/sjiang18/data/plans/wiki.title'
	get_title(infilename, outfilename)
	print datetime.now().time()
