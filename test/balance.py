import math,sys
B=[("Limonade",15,0.5),("Boulangerie",100,3),("Pizzeria",1100,20),("Cinema",12000,110),("Usine",130000,700),
   ("Banque",1.4e6,4000),("Studio",2e7,25000),("Fusee",3.3e8,150000),("Mars",5.1e9,1e6)]
MS=[25,50,100,200,300,400,500]
ups=[]
for b,(n,c,i) in enumerate(B):
    for tc,tm in ((25,2),(2500,3),(250000,4)): ups.append((c*tc,"B",b,tm))
ups+= [(100,"C",0,2),(5000,"C",0,3),(50000,"S",0,0),(5e6,"C",0,5),(5e9,"C",0,10),
       (1e6,"A",0,1.5),(5e8,"A",0,2),(5e10,"A",0,2),(5e12,"A",0,3)]
def run(cps, inv=0, T=4*3600, label=""):
    cnt=[0]*9; bought=set(); money=0; run_=0; t=0; dt=1
    marks={}
    def bm(b):
        m=2**sum(cnt[b]>=x for x in MS)
        for k,u in enumerate(ups):
            if k in bought and u[1]=="B" and u[2]==b: m*=u[3]
        return m
    def gm():
        m=1+0.02*inv
        for k,u in enumerate(ups):
            if k in bought and u[1]=="A": m*=u[3]
        return m
    def inc(): return sum(cnt[b]*B[b][2]*bm(b) for b in range(9))*gm()
    def clk():
        c=1; s=False
        for k,u in enumerate(ups):
            if k in bought:
                if u[1]=="C": c*=u[3]
                if u[1]=="S": s=True
        return c*gm()+(inc()*0.02 if s else 0)
    while t<T:
        I=inc(); c = clk()*cps(t)
        money+=(I+c)*dt; run_+=(I+c)*dt; t+=dt
        for th,name in ((1e3,"1K"),(1e6,"1M"),(1e7,"10M"),(1e9,"1B"),(1e12,"1T")):
            if run_>=th and name not in marks: marks[name]=t
        # achat greedy : meilleur rapport gain/prix
        while True:
            best=None
            for b in range(9):
                if b>0 and cnt[b-1]==0 and cnt[b]==0: continue
                p=B[b][1]*1.15**cnt[b]
                cnt[b]+=1; gain=inc()-I; cnt[b]-=1
                sc=gain/p
                if best is None or sc>best[0]: best=(sc,"b",b,p)
            for k,u in enumerate(ups):
                if k in bought: continue
                if u[1]=="B" and cnt[u[2]]==0: continue
                bought.add(k); g=inc()-I + (clk()*cps(t)-c); bought.discard(k)
                sc=g/u[0]
                if best is None or sc>best[0]: best=(sc,"u",k,u[0])
            if best and best[3]<=money:
                money-=best[3]
                if best[1]=="b": cnt[best[2]]+=1
                else: bought.add(best[2])
                I=inc(); c=clk()*cps(t)
            else: break
        if cnt[8]>0 and "Mars" not in marks: marks["Mars"]=t
    g=math.floor(10*math.sqrt(run_/1e9)) if run_>=1e7 else 0
    print(label, {k:f"{v//60}m{v%60:02d}" for k,v in marks.items()}, "cnt",cnt, f"run={run_:.3g}", "investors",g)
    return g
run(lambda t: 3 if t<600 else 0, T=3600, label="1h, clique 10 min:")
run(lambda t: 3 if t<600 else 0, T=3*3600, label="3h:")
