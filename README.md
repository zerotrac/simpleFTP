# simpleFTP

放一个文档在这里，防止自己犯拖延症= =

## UDP
已完成

## TCP-Server

### Fundamental

- (已完成) USER, PASS, SYST, TYPE, QUIT, ABOR
- (已完成) PORT, PASV, RETR, STOR
- (已完成) MKD, RMD, CWD, LIST
- (已完成) Autograde
- (已完成) 自测无 bug
    - `mkd <DIR> cwd <DIR> rmd ../<DIR> cwd ..` 会挂，不想处理了，这要写`cwd ..`的手动解析...
    - `retr ..`可能会挂，哇都是`..`的锅

### Optional

- (已完成) kqueue 完全异步
    - 真·异步，文件传输都是异步
- (已完成) 绝对/相对路径支持 & 权限检查
    - 假·路径支持，放`chdir() getcwd()`里面跑一下的，并没有手动解析
- (已完成) 命令大小写兼容
- (已完成) PWD

## TCP-Client

### Fundamental

- (懈怠中) USER, PASS, SYST, TYPE, QUIT, ABOR
- (懈怠中) PORT, PASV, RETR, STOR
- (懈怠中) MKD, RMD, CWD, LIST
- (懈怠中) 自测无 bug

### Optional

- (懈怠中) 简易 shell
- (懈怠中) PWD

## Report

- (懈怠中) 文档
