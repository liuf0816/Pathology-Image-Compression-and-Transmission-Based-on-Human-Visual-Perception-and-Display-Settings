function [res1,hdrvdpdb] = hdrvdpdb(args)
% Wrapping script for the HDR-VDP image comparison metric,
% as described in "A comparison of Three Image Fidelity Metrics of
% Different Computational Principles for JPEG2000
% Compressed Abdomen CT Images", Kil Joong Kim, ..., Thomas Richter, et al, TMI 2010
%  
% input: args.original_path, args.distorted_path
% return: the HDR-VDP in dB
%  
% @author Miguel Hernández Cabronero <mhernandez@deic.uab.cat>
% @date 22/05/2014

% max_intensity = 2^16 - 1
max_intensity = args.max_intensity;
beta = 2.4;

% addpath('/opt/matlab/hdrvdp');
addpath([pwd '\hdrvdp']);
% addpath('/opt/matlab/matlabPyrTools');
% addpath('/opt/matlab/matlabPyrTools/MEX');

%  Read image and obtain probability map

% original_image = double(imread(args.original_path))/max_intensity;
% distorted_image = double(imread(args.distorted_path))/max_intensity;
original_image = double(imread(args.original_path));
distorted_image = double(imread(args.distorted_path));

% see hdrvdp.m for details about the mode
if strcmp(args.mode, 'luma-display')
  original_image = original_image / max_intensity;
  distorted_image = distorted_image / max_intensity;
end
if strcmp(args.mode, 'sRGB-display')
  original_image = original_image / max_intensity;
  distorted_image = distorted_image / max_intensity;
end


tamanyo = size(original_image);
ppd = hdrvdp_pix_per_deg( 21, [tamanyo(2) tamanyo(1)], 1 );
res1 = hdrvdp( distorted_image, original_image, args.mode, ppd );
%  Once the probability map is obtained, the HDR-VDP in dB is calculated
hdrvdp_max = (tamanyo(1)*tamanyo(2))^(1/beta);
hdrvdp_denominator = sum(sum(res1.P_map.^beta))^(1/beta);
hdrvdpdb = 20 * log10(hdrvdp_max/hdrvdp_denominator);