良好的 Git 分支命名规范能够帮助团队快速识别分支用途、提升协作效率，并让代码审查和自动化流程更加顺畅。综合业界最佳实践，以下是 Git 分支命名的核心规范和常见分类：

### 一、 核心命名规则
在构建分支名称时，通常需要遵循以下基础原则，以确保兼容性和可读性：
1. **全小写与连字符分隔**：始终使用小写字母，单词之间使用连字符（`-`）分隔，避免使用空格、下划线或驼峰命名。
2. **字符限制**：仅使用小写字母（a-z）、数字（0-9）和连字符（-）。避免使用特殊字符（如 `!@#$%^&*()`）。
3. **格式规范**：不要使用连续的连字符（如 `new--login`），也不要在分支名称的末尾添加连字符。
4. **简洁且具描述性**：分支名应简短明了，让人一眼就能看懂该分支正在进行的工作。

### 二、 标准命名结构
`<type>/<ticket-id>-<short-description>` （类型/工单号-简短描述）

*   **type（类型）**：说明该分支的工作性质。
*   **ticket-id（工单号）**：关联 Jira 或 GitHub Issue 等任务管理工具的编号，便于追踪。
*   **short-description（简短描述）**：用几个词概括具体任务。

### 三、 分支类型及示例
根据工作内容的不同，通常使用以下前缀进行分类：

1. **feature/ （新功能）**：用于开发新的功能模块。
   * *示例*：`feature/add-login-page`、`feature/PROJ-123-user-authentication`
2. **bugfix/ 或 fix/ （常规缺陷修复）**：用于修复非紧急的 Bug。
   * *示例*：`bugfix/fix-header-bug`、`bugfix/ABC-456-fix-null-pointer`
3. **hotfix/ （紧急热修复）**：用于修复生产环境中的紧急问题或安全漏洞。
   * *示例*：`hotfix/security-patch`、`hotfix/critical-payment-gateway`
4. **release/ （发布准备）**：用于准备新版本发布，通常包含版本号。
   * *示例*：`release/v1.2.0`、`release/2024-spring`
5. **chore/ （日常杂项）**：用于非业务代码的修改，如更新依赖、修改文档、配置调整等。
   * *示例*：`chore/update-dependencies`、`chore/PROJ-101-dependency-update`
6. **experiment/ 或 wip/ （实验性/进行中）**：用于尝试新想法、重构或尚未完成的工作。
   * *示例*：`experiment/new-ui-design`、`wip-ioc-container-added`
