# CI 配置说明

这个目录里放了两种常见 CI 平台的模板：

- `github-actions-ci.yml`：如果你们使用 GitHub，把这个文件放到仓库的 `.github/workflows/` 目录下。
- `gitlab-ci.yml`：如果你们使用 GitLab，把这个文件放到仓库根目录，并改名为 `.gitlab-ci.yml`。
- `run_ci.sh`：GitLab Linux 环境使用的一键运行脚本。

不管用哪个平台，CI 实际只做两件事：

1. 编译 `rich_test.exe`（或 Linux 下的 `rich_test`）；
2. 运行 `testcases/iteration1/` 下所有 JSON 用例。

如果你们使用 Gitee、Jenkins 或公司内部平台，也只需要让 CI 执行同样的两条命令：

```powershell
.\build_cl.bat
.\run_tests.ps1
```

如果测试还没有通过，CI 会显示红色，这正是测试驱动开发前期的正常状态。
