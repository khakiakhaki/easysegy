# SEG-Y File I/O Library

这是一个轻量级的 C 语言库，用于高效读写 SEG-Y 格式的地震数据文件。该库提供了简洁的 API 来操作 SEG-Y 文件的文本头、二进制头和数据道。

## Design Philosophy 设计理念

### Reading Mode 读取模式
- `segyfile_init_read()` 初始化时会自动将文件指针定位到**二进制文件头的起始位置**
- 初始化过程会自动读取**文本头**和**二进制头**
- 读取数据道需要显式调用 `segyread_onetrace()` 函数

### Writing Mode 写入模式
- `segyfile_init_write()` 初始化时将文件指针定位到**文件起始位置**
- **不会自动写入任何头信息**
- 需要手动调用：
  - `segywrite_texthead()` 写入文本头
  - `segywrite_binhead()` 写入二进制头
  - `segywrite_onetrace()` 写入数据道

这样的设计允许用户：
1. 在写入前灵活修改头信息
2. 精确控制写入顺序
3. 实现自定义的头信息处理逻辑

## Key Features 主要特性
- **轻量级**: 无外部依赖，纯 C 实现
- **灵活**: 允许自定义头信息处理
- **符合标准**: 支持标准 SEG-Y Rev1 格式

## Building 构建
```bash
# 编译库
gcc -c segylib.c -o segylib.o
ar rcs libesegy.a segylib.o

# 链接示例程序
gcc example.c -L. -lsegy -o example
```

## License 许可
MIT License - 允许自由使用和修改