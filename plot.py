
import matplotlib.pyplot as plt

# before running this should be put into the obj folder: 
# where the csv file will be generated

fp=open("cwnd.csv")
cwnd=[]
time=[]

for line in fp:
	line=line.strip().split(":")
	time.append(int(line[0])) 
	cwnd.append(int(line[1]))


fig = plt.figure(figsize=(20,5), dpi=300)
plt.plot(time, cwnd, lw=2,color='maroon')


plt.title("Congestion Window Over Time", fontsize=20)

plt.xlabel("Time", fontsize=15)
plt.ylabel("Congestion Window", fontsize=15)
# plt.tick_params(axis = 'both', which = 'major' , labelsize = 16)


plt.savefig("cwnd-plot.pdf", dpi=500, bbox_inches='tight')

plt.show()
fp.close()







