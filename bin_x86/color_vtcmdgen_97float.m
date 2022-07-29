function color_vtcmdgen_97float(par1,par2)
b_lum=0.01*ones(1,16)/256;
b_c1=b_lum;
b_c2=b_lum;


vtpar='_VT1.j2c Qabs_steps=';
vtpar=[vtpar num2str(b_lum(1),100)];
for ki=2:16
    vtpar=[vtpar ',' num2str(b_lum(ki),100)];
end
vtpar=[vtpar ' Qabs_steps:C1=' num2str(b_c1(1),100)];
for ki=2:16
    vtpar=[vtpar ',' num2str(b_c1(ki),100)];
end
vtpar=[vtpar ' Qabs_steps:C2=' num2str(b_c2(1),100)];
for ki=2:16
    vtpar=[vtpar ',' num2str(b_c2(ki),100)];
end
vtpar=[vtpar ' -no_weights -slope 1'];

image_folder=[pwd '\val_images\'];
listing=dir(fullfile([image_folder 'decodedvt0\'],'*.ppm'));

bitrate=zeros(1,length(listing));
PSNR=zeros(1,length(listing));
HDRVDP_cal=zeros(1,length(listing));
HDRVDP_map=cell(1,length(listing));
for ki=1:length(listing)
    nm=listing(ki).name;
    cmd=['kdu_compress -i ' image_folder 'decodedvt0\' nm ' -o ' image_folder sprintf('jp2foldervt1_%d%d',par1,par2) '\' nm vtpar];
    system(cmd);
    cmd=['kdu_expand -i ' image_folder sprintf('jp2foldervt1_%d%d',par1,par2) '\' nm '_VT1.j2c -o ' image_folder sprintf('decodedvt1_%d%d',par1,par2) '\' nm '_VT1.j2c.ppm'];
    system(cmd);
    D = dir([image_folder sprintf('jp2foldervt1_%d%d',par1,par2) '\' nm '_VT1.j2c']);
    size_VT = D.bytes;
    I0=imread([image_folder 'decodedvt0\' nm],'PPM');
    I=imread([image_folder sprintf('decodedvt1_%d%d',par1,par2) '\' nm '_VT1.j2c.ppm'],'PPM');
    bitrate(ki)=size_VT*8/(size(I0,1)*size(I0,2));
    PSNR(ki)=10*log10( 255^2/mean((double(I(:))-double(I0(:))).^2));
    args.original_path=[image_folder 'decodedvt0\' nm];
    args.distorted_path=[image_folder sprintf('decodedvt1_%d%d',par1,par2) '\' nm '_VT1.j2c.ppm'];
    args.max_intensity=255;
    args.mode='sRGB-display';
    [HDRVDP_map{ki},HDRVDP_cal(ki)]=hdrvdpdb(args);
end

save(sprintf('color_stat1_%d%d.mat',par1,par2),'bitrate','PSNR','HDRVDP_map','HDRVDP_cal');