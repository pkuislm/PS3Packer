# Ps3Packer

用于打包PurpleSoftware的PS3脚本的工具

PS3脚本包含代码段、附加段以及文本块，由于个人水平原因，未能完整解析所有opcode，因此为避免错误偏移造成脚本错误，回封时会将文本附加在原始文本块的后部，并只对翻译文本内存在的文本进行偏移的修改。

#### 注意：脚本回封时仍然使用压缩，但已移除加密。您需要修改主程序的解密部分，或手动添加加密函数

此工具接受的文本格式请参考<https://github.com/AyamiKaze/GalgameNote/blob/main/cmvs_hook_tuts/CMVS%E4%B8%AD%E6%96%87%E5%8C%96%E6%94%AF%E6%8C%81%E2%80%94%E2%80%94%E4%BB%A5%E5%A4%A9%E6%B4%A5%E7%BD%AA%E4%B8%BA%E4%BE%8B%EF%BC%8C%E7%94%A8hook%E6%B1%89%E5%8C%96%E6%89%80%E6%9C%89%E5%86%85%E5%AE%B9.md>中对<https://github.com/xmoeproject/CMVS-Engine/tree/master/Ps3TextDumper>进行的改动

使用方法：将txt格式，utf8无签名格式的文本与已经解压且解密后的ps3文件放置于同一目录下，将单个/多个ps3文件拖拽到exe上即可