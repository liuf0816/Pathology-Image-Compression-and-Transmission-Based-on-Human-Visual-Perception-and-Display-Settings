可执行文件位于bin文件夹

-JNDR 固定分辨率下的视觉质量分层

   kdu_compress -i input.ppm -o out.jp2 -JNDR ( jnd1,jnd2,...,jndN,r )

示例：
   kdu_compress -i input.ppm -o out.jp2 -JNDR 0.1,0.5,1,1
   此时图片有三个质量层，质量层1对应分辨率为1，视觉质量为0.1的图片，质量层2对应分辨率为1，视觉质量为0.5的图片，质量层3对应分辨率为1，视觉质量为1的图片

-RJND 固定视觉质量下的分辨率分层

  kdu_compress -i input.ppm -o out.jp2 -RJND ( r1,r2,...,rN,jnd )

  和-JNDR相似，不可以与-JNDR同时使用

-Pmap 使用概率图对vt进行加权从而实现感兴趣区域

  kdu_compress -i input.ppm -o out.jp2 -Pmap map.pgm

  概率图的文件格式必须是pgm格式，且大小必须和ppm对应。
  可以与-JNDR或-RJND一同使用。

-extralayer

  kdu_compress -i input.ppm -o out.jp2 -JNDR 0.5,1,1 -Pmap map.pgm -extralayer

  只能和-Pmap一同使用
  当使用这个选项时，会自动增加一层质量层，最后一个质量层会恢复所有区域的视觉质量，其他的质量层则区分ROI和BG。

-fastmode 快速计算VT
 
 kdu_compress -i input.ppm -o out.jp2 -JNDR 0.5,1,1 -Pmap map.pgm -fastmode

 对于超大图片来说，对不同的码块分别计算VT效率很低，使用该选项将采用WSI的平均小波系数标准差对VT进行计算，以提升编码效率，但视觉质量会稍微降低
  