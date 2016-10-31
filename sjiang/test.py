import threading, Queue

term_freq = {}

def get_freq(words, k, j):
	n = 0
	for w in words:
		if w not in term_freq:
			n += 1
			term_freq[w] = 1
		else:
			term_freq[w] += 1
	return n, len(words)

if __name__=='__main__':
	words1 = ['a', 'b', 'c', 'd', 'e']
	words2 = ['b', 'c', 'd', 'e', 'f']
	words3 = ['c', 'd', 'e', 'f', 'g']
	que = Queue.Queue()
	t1 = threading.Thread(target = lambda q, (arg1, arg2, arg3) : q.put(get_freq(arg1, arg2, arg3)), args = (que, (words1, 1, 2)))
	t2 = threading.Thread(target = lambda q, (arg1, arg2, arg3) : q.put(get_freq(arg1, arg2, arg3)), args = (que, (words2, 2, 3)))
	t3 = threading.Thread(target = lambda q, (arg1, arg2, arg3) : q.put(get_freq(arg1, arg2, arg3)), args = (que, (words3, 3, 4)))
	t1.start()
	t2.start()
	t3.start()
	t1.join()
	t2.join()
	t3.join()
	result1, result2 = 0, 0
	while not que.empty():
		a = que.get()
    		print a
		result1 += a[0]
		result2 += a[1]
	print result1, result2
	print term_freq
'''
def dosomething(param):
    return param * 2
que = Queue.Queue()
thr = threading.Thread(target = lambda q, arg : q.put(dosomething(arg)), args = (que, 2))
thr.start()
thr.join()
while not que.empty():
    print(que.get())
'''
