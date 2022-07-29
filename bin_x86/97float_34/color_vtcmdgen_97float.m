b1_lum=[0.5004 0.5004 0.4998 0.5758 0.5758 0.5052 51.067 51.067 4.1457 68.55 68.55 29.9467 82.42 82.42 38.856];
b1_c1=[0.4997 0.4997 0.4997 0.5 0.5 0.4997 0.503 0.503 0.5011 0.5075 0.5075 0.5036 8.1682 8.1682 0.5253 0.5535];
b1_c2=[0.4998 0.4998 0.4997 0.5012 0.5012 0.5001 0.5172 0.5172 0.5053 2.764 2.764 0.5486 10.6069 10.6069 3.6764 0.62];
b1_lum_ll5=0.585097338836604;

b_lum=[b1_lum_ll5/256 b1_lum/256];
b_c1=b1_c1/256;
b_c2=b1_c2/256;
b_lum_out=[b1_lum_ll5 b1_lum];
b_c1_out=b1_c1;
b_c2_out=b1_c2;
a_lum_out=zeros(1,16);


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

image_folder='E:\study\JPEG2000\visually_lossless_JPEG2000\bin_x86\color\';
listing=dir(fullfile([image_folder 'decodedvt0\'],'*.ppm'));

bitrate=zeros(1,length(listing));
PSNR=zeros(1,length(listing));

for ki=1:length(listing)
    nm=listing(ki).name;
    cmd=['E:\study\JPEG2000\visually_lossless_JPEG2000_ori\v6_generated\compress\debug\kdu_compress.exe -i ' image_folder 'decodedvt0\' nm ' -o ' image_folder 'jp2foldervt1' '\' nm vtpar];
    system(cmd);
    cmd=['E:\study\JPEG2000\visually_lossless_JPEG2000\v6_generated\expand\debug\kdu_expand.exe -i ' image_folder 'jp2foldervt1' '\' nm '_VT1.j2c -o ' image_folder 'decodedvt1' '\' nm '_VT1.j2c.ppm'];
    system(cmd);
    D = dir([image_folder 'jp2foldervt1\' nm '_VT1.j2c']);
    size_VT = D.bytes;
    I0=imread([image_folder 'decodedvt0\' nm],'PPM');
    I=imread([image_folder 'decodedvt1\' nm '_VT1.j2c.ppm'],'PPM');
    bitrate(ki)=size_VT*8/(size(I0,1)*size(I0,2));
    PSNR(ki)=10*log10( 255^2/mean((double(I(:))-double(I0(:))).^2));
end

save('color_stat1.mat','bitrate','PSNR');