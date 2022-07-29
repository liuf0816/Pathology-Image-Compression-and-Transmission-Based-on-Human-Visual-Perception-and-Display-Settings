b1_lum=[0.422400326487246 0.422400326487246 0.420964852041254 0.480091534214651 0.480091534214651 0.519440181686338 0.617577388213508 0.617577388213508 0.695306082092589 0.942720413311635 0.942720413311635 1.31702625461010 2.14744974853953 2.14744974853953 128];
b1_lum_ll5=0.639695563615282;

b_lum=[b1_lum_ll5/256 b1_lum/256];
b_lum_out=[b1_lum_ll5 b1_lum];
a_lum_out=zeros(1,16);

vtpar='_VT1.j2c Qabs_steps=';
vtpar=[vtpar num2str(b_lum(1),100)];
for ki=2:16
    vtpar=[vtpar ',' num2str(b_lum(ki),100)];
end
vtpar=[vtpar ' -slope 1'];

image_folder='E:\visually_lossless_JPEG2000\visually_lossless_JPEG2000\visually_lossless_JPEG2000\bin_x86\es_images\gray\';
listing=dir(fullfile([image_folder 'decodedvt0\'],'*.pgm'));

bitrate=zeros(1,length(listing));
PSNR=zeros(1,length(listing));

for ki=1:length(listing)
    nm=listing(ki).name;
    cmd=['kdu_compress -i ' image_folder 'decodedvt0\' nm ' -o ' image_folder 'jp2foldervt1' '\' nm vtpar];
    system(cmd);
    cmd=['kdu_expand -i ' image_folder 'jp2foldervt1' '\' nm '_VT1.j2c -o ' image_folder 'decodedvt1' '\' nm '_VT1.j2c.pgm'];
    system(cmd);
    D = dir([image_folder 'jp2foldervt1\' nm '_VT1.j2c']);
    size_VT = D.bytes;
    I0=imread([image_folder 'decodedvt0\' nm],'PGM');
    I=imread([image_folder 'decodedvt1\' nm '_VT1.j2c.pgm'],'PGM');
    bitrate(ki)=size_VT*8/(size(I0,1)*size(I0,2));
    PSNR(ki)=10*log10( 255^2/mean((double(I(:))-double(I0(:))).^2));
end

save('gray_stat1.mat','bitrate','PSNR');