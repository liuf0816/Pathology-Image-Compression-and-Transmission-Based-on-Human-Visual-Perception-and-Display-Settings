b1_lum=[0.422400326487246 0.422400326487246 0.420964852041254 0.480091534214651 0.480091534214651 0.519440181686338 0.617577388213508 0.617577388213508 0.695306082092589 0.942720413311635 0.942720413311635 1.31702625461010 2.14744974853953 2.14744974853953 128];
b1_c1=[0.567612563944478 0.436102460279300 0.436102460279300 0.508930992545194 0.631015818309502 0.631015818309502 0.833508229803184 1.35655646777036 1.35655646777036 128 128 128 128 128 128 128];
b1_c2=[0.604063459557658 0.526801492931860 0.526801492931860 0.560009606971177 0.703942633251875 0.703942633251875 1.00370672325915 1.17948987597535 1.17948987597535 1.67041694007242 2.00296250343579 2.00296250343579 128 128 128 128];
b1_lum_ll5=0.639695563615282;

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

image_folder='E:\visually_lossless_JPEG2000\visually_lossless_JPEG2000\visually_lossless_JPEG2000\bin_x86\es_images\color\';
listing=dir(fullfile([image_folder 'decodedvt0\'],'*.ppm'));

bitrate=zeros(1,length(listing));
PSNR=zeros(1,length(listing));

for ki=1:length(listing)
    nm=listing(ki).name;
    cmd=['kdu_compress -i ' image_folder 'decodedvt0\' nm ' -o ' image_folder 'jp2foldervt1' '\' nm vtpar];
    system(cmd);
    cmd=['kdu_expand -i ' image_folder 'jp2foldervt1' '\' nm '_VT1.j2c -o ' image_folder 'decodedvt1' '\' nm '_VT1.j2c.ppm'];
    system(cmd);
    D = dir([image_folder 'jp2foldervt1\' nm '_VT1.j2c']);
    size_VT = D.bytes;
    I0=imread([image_folder 'decodedvt0\' nm],'PPM');
    I=imread([image_folder 'decodedvt1\' nm '_VT1.j2c.ppm'],'PPM');
    bitrate(ki)=size_VT*8/(size(I0,1)*size(I0,2));
    PSNR(ki)=10*log10( 255^2/mean((double(I(:))-double(I0(:))).^2));
end

save('color_stat1.mat','bitrate','PSNR');