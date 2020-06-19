1.qt默认显示rgb32, 针对qt显示做了如下优化:
	1).去掉透明, 帧率提升明显, 由BR2_PACKAGE_QT5BASE_LINUXFB_RGB32 宏控制.

	2).优化drawImage, 使用rga 合成, 进一步提升帧率, 降低cpu, 由BR2_PACKAGE_QT5BASE_USE_RGA 宏控制.

	3).camera数据直接绘制到fb buf, 跳过涂黑以及后面的两次neon合成, 进一步降低cpu,由BR2_PACKAGE_QT5BASE_LINUXFB_DIRECT_PAINTING 宏控制, 需要注意的是该优化只在单窗口效.

	4).以上宏开关均可在根目录运行make menuconfig 修改, 修改后需要make savedefconfig 保存配置, 并make qt5base-dirclean && make qt5base-rebuild, make QFacialGate-rebuild 使修改生效.

	5).BR2_PACKAGE_QT5BASE_LINUXFB_RGB32 和 BR2_PACKAGE_QT5BASE_USE_RGA 必须开启,否则帧率下降严重, 画面会有明显卡顿.

	6).如果UI使用多窗口, 可关闭BR2_PACKAGE_QT5BASE_LINUXFB_DIRECT_PAINTING, 但是cpu占用会升高, 可能导致帧率下降.

2.QFacialGate UI 优化
	rga 合成显示人脸识别信息的底部半透阴影框, 代替直接使用qt 的drawRect, 以便提升帧率, 降低cpu, 如果UI有类似的大面积阴影显示, 请参考QFacialGate 使用rga合成.
