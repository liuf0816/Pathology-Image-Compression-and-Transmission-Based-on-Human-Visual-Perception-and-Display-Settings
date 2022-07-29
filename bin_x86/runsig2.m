M=textread('thresholds.txt');
sig2=zeros(16,3);
sig2_cont=zeros(16,3);
for ki=0:2
    for kj=1:3
        for kz=1:5
            Mtmp=M(((M(:,1)==ki)&(M(:,2)==kj)&(M(:,3)==kz)),4);
            sig2((kz-1)*3+4-kj,ki+1)=mean(Mtmp(:));
            sig2_cont((kz-1)*3+4-kj,ki+1)=sqrt(var(Mtmp(:)))/mean(Mtmp(:));
        end
    end
    Mtmp=M(((M(:,1)==ki)&(M(:,2)==0)&(M(:,3)==5)),4);
    sig2(16,ki+1)=mean(Mtmp(:));
    sig2_cont(16,ki+1)=sqrt(var(Mtmp(:)))/mean(Mtmp(:));
end
sig2tmp=zeros(11,3);
sig2tmp(1:2:9,:)=sig2(1:3:13,:);
sig2tmp(2:2:10,:)=(sig2(2:3:14,:)+sig2(3:3:15,:))/2;
sig2tmp(end,:)=sig2(16,:);