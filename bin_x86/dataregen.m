stat=zeros(9,3,4,3);
Q=[0.1 0.3 0.7 1];
r=[0.5 0.75 1];
image_folder=[pwd '\val_images\'];
image_folder2=[pwd '\test_images\'];
load('listing.mat');
for ki=1:3
    for kj=1:3
        for kz=1:4
            load(sprintf('color_stat%d_%d%d.mat',kj,kz,ki));
            HDRVDP_cal=zeros(1,101);
            HDRVDP_map=cell(1,101);
            PSNR=zeros(1,101);
            bitrate=zeros(1,101);
            for km=1:101
                nm=listing(km).name;
                I=imread([image_folder sprintf('decodedvt%d_%d%d',kj,kz,ki) '\' nm '_VT1.j2c.ppm'],'PPM');
                I0=imread([image_folder 'decodedvt0\' nm],'PPM');
                if(kj~=1)
                    ygrid=(1:size(I,1))-round(size(I,1)/2);
                    xgrid=(1:size(I,2))-round(size(I,2)/2);
                    [x,y]=meshgrid(xgrid,ygrid);
                    xgridv=xgrid/r(kj);
                    xgridv=xgridv((xgridv>=xgrid(1))&(xgridv<=xgrid(end)));
                    ygridv=ygrid/r(kj);
                    ygridv=ygridv((ygridv>=ygrid(1))&(ygridv<=ygrid(end)));
                    [xv,yv]=meshgrid(xgridv,ygridv);
                    if((ki==1)&&(kz==1))
                        I0=zeros(length(yv(:)),length(xv(:)),3);
                        I0(:,:,1)=interp2(x,y,double(I0(:,:,1)),xv,yv);
                        I0(:,:,2)=interp2(x,y,double(I0(:,:,2)),xv,yv);
                        I0(:,:,3)=interp2(x,y,double(I0(:,:,3)),xv,yv);
                        I0=uint8(I0);
                    end
                    I=zeros(length(yv(:)),length(xv(:)),3);
                    I(:,:,1)=interp2(x,y,double(I(:,:,1)),xv,yv);
                    I(:,:,2)=interp2(x,y,double(I(:,:,2)),xv,yv);
                    I(:,:,3)=interp2(x,y,double(I(:,:,3)),xv,yv);
                    I=uint8(I);
                end
                if((ki==1)&&(kz==1))
                    imwrite(I0,[image_folder2 sprintf('decodedvt0_%d',kj) '\' nm],'PPM');
                end
                imwrite(I,[image_folder2 sprintf('decodedvt%d_%d%d',kj,kz,ki) '\' nm '_VT1.j2c.ppm'],'PPM');
                I0=imread([image_folder2 sprintf('decodedvt0_%d',kj) '\' nm],'PPM');
                I=imread([image_folder2 sprintf('decodedvt%d_%d%d',kj,kz,ki) '\' nm '_VT1.j2c.ppm'],'PPM');
                args.original_path=[image_folder2 sprintf('decodedvt0_%d',kj) '\' nm];
                args.distorted_path=[image_folder2 sprintf('decodedvt%d_%d%d',kj,kz,ki) '\' nm '_VT1.j2c.ppm'];
                args.max_intensity=255;
                args.mode='sRGB-display';
                [HDRVDP_map{km},HDRVDP_cal(km)]=hdrvdpdb(args);
                PSNR(km)=10*log10( 255^2/mean((double(I(:))-double(I0(:))).^2));
                bitrate(km)=bitrate(km)*length(ygrid)/length(ygridv);
            end
            save(sprintf('color_stat_new%d_%d%d.mat',kj,kz,ki),'bitrate','PSNR','HDRVDP_map','HDRVDP_cal');
            stat(1,kj,kz,ki)=mean(HDRVDP_cal(:));
            stat(2,kj,kz,ki)=min(HDRVDP_cal(:));
            stat(3,kj,kz,ki)=max(HDRVDP_cal(:));
            stat(4,kj,kz,ki)=mean(bitrate(:));
            stat(5,kj,kz,ki)=min(bitrate(:));
            stat(6,kj,kz,ki)=max(bitrate(:));
            stat(7,kj,kz,ki)=mean(PSNR(:));
            stat(8,kj,kz,ki)=min(PSNR(:));
            stat(9,kj,kz,ki)=max(PSNR(:));
        end
    end
end
[Q,r]=meshgrid(Q,r);
figure;
mesh(Q,r,squeeze(stat(1,:,:,1)));
figure;
mesh(Q,r,squeeze(stat(1,:,:,2)));
figure;
mesh(Q,r,squeeze(stat(1,:,:,3)));
figure;
mesh(Q,r,squeeze(stat(4,:,:,1)));
figure;
mesh(Q,r,squeeze(stat(4,:,:,2)));
figure;
mesh(Q,r,squeeze(stat(4,:,:,3)));