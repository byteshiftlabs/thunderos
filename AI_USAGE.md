# AI Usage Policy for ThunderOS

## Overview

ThunderOS **embraces AI-assisted development** as a powerful tool for accelerating development, improving code quality, and enhancing documentation. This document outlines our policies and best practices for using AI in this project.

## Core Principle

> **AI is permitted at 100%, provided you understand what you're committing to the repository.**

The use of AI tools (GitHub Copilot, ChatGPT, Claude, etc.) is not just allowed but **encouraged** in ThunderOS development. However, developers must take full responsibility for understanding and validating all AI-generated code and documentation.

## Guidelines

### ✅ What's Allowed

1. **Code Generation**
   - Using AI to generate boilerplate code, function implementations, and algorithms
   - AI-assisted refactoring and optimization
   - Generating test cases and test frameworks
   - Converting pseudocode or specifications into actual code

2. **Documentation**
   - AI-generated documentation for functions, modules, and APIs
   - README and markdown file creation/improvement
   - Code comments and inline explanations
   - Technical writing and tutorials

3. **Problem Solving**
   - Debugging assistance and error analysis
   - Architecture and design suggestions
   - Performance optimization recommendations
   - Security vulnerability detection

4. **Research**
   - Understanding RISC-V specifications and assembly
   - Learning OS development concepts
   - Exploring algorithms and data structures
   - Investigating best practices

### ⚠️ Requirements

1. **Understanding is Mandatory**
   - You **must** understand every line of AI-generated code before committing
   - If you don't understand something, research it or ask for clarification
   - Never blindly copy-paste AI output without comprehension

2. **Testing is Required**
   - All AI-generated code must be tested thoroughly
   - Verify that the code works as intended
   - Check edge cases and error handling
   - Run the existing test suite to ensure no regressions

3. **Review and Validate**
   - Review AI-generated documentation for accuracy
   - Ensure code follows ThunderOS coding standards
   - Check for potential bugs, security issues, or inefficiencies
   - Verify that architectural decisions align with project goals

4. **Attribution (Optional but Appreciated)**
   - While not required, you may note significant AI assistance in commit messages
   - Example: `git commit -m "Implement virtual memory paging (AI-assisted)"`
   - This helps track the effectiveness of AI tools in the project

### ❌ What's Not Acceptable

1. **Blind Copy-Paste**
   - Committing code you don't understand
   - Merging AI suggestions without reviewing them
   - Ignoring compiler warnings or errors in AI-generated code

2. **Replacing Judgment**
   - Letting AI make architectural decisions without your oversight
   - Accepting AI suggestions that conflict with project design
   - Ignoring your own expertise in favor of AI output

3. **Security Negligence**
   - Not reviewing AI-generated code for security vulnerabilities
   - Trusting AI-generated cryptographic or security-critical code without expert validation
   - Skipping security best practices because "AI generated it"

4. **License Violations**
   - Using AI to generate code that infringes on copyrights
   - Not verifying that AI output is compatible with ThunderOS license (check LICENSE file)
   - Copying code from AI that was trained on incompatible licenses

## Best Practices

### For Code

1. **Incremental Approach**
   - Generate small chunks of code at a time
   - Test each piece before moving to the next
   - Build understanding progressively

2. **Critical Review**
   - Question AI suggestions critically
   - Compare with existing codebase patterns
   - Verify against RISC-V specifications when applicable

3. **Documentation Sync**
   - Ensure AI-generated code is properly documented
   - Update existing documentation if AI changes affect it
   - Keep comments and code in sync

### For Documentation

1. **Accuracy Check**
   - Verify that AI-generated docs match the actual code
   - Test code examples in documentation
   - Ensure technical accuracy (especially for RISC-V details)

2. **Clarity and Consistency**
   - Review for clear, understandable language
   - Maintain consistent terminology throughout docs
   - Ensure style matches existing documentation

3. **Completeness**
   - Fill in gaps AI might miss
   - Add context and rationale that AI can't infer
   - Include examples and edge cases

## Documentation Review Process (Code Freeze Periods)

During code freeze periods (like v0.2.0), all AI-generated documentation will be reviewed for:

1. **Accuracy**: Does it match the actual code?
2. **Completeness**: Are all features and modules documented?
3. **Clarity**: Is it understandable for both users and developers?
4. **Consistency**: Is terminology and style consistent?
5. **Technical Correctness**: Are explanations technically sound?

This is particularly important because AI may:
- Hallucinate features that don't exist
- Misunderstand complex architectural decisions
- Generate outdated documentation based on old training data


## Examples of Good AI Usage

### Example 1: Implementing a New Feature
```
Developer: I need to implement copy-on-write for memory pages
↓
AI: Generates initial implementation with fork() support
↓
Developer: Reviews code, understands COW mechanism
         Tests with multiple processes
         Adjusts to ThunderOS memory management style
         Documents the implementation
↓
Result: Well-understood, tested, and documented feature
```

### Example 2: Documentation
```
Developer: The paging.c file needs API documentation
↓
AI: Generates function documentation and module overview
↓
Developer: Verifies accuracy against actual code
         Adds usage examples from tests
         Corrects AI misconceptions about page table format
         Ensures consistency with existing docs
↓
Result: Accurate, complete documentation
```

### Example 3: Debugging
```
Developer: Getting a page fault I don't understand
↓
AI: Analyzes register dump and suggests possible causes
↓
Developer: Understands the issue (misaligned page table entry)
         Fixes the bug
         Adds test case to prevent regression
         Updates comments to clarify alignment requirements
↓
Result: Bug fixed, codebase improved
```

## Version-Specific Considerations

### v0.2.0 and Beyond: System Calls

When implementing user-space and syscalls with AI assistance:
- Verify privilege level transitions (S-mode ↔ U-mode)
- Ensure proper parameter validation (user pointers!)
- Check for security vulnerabilities in syscall handlers
- Test with malicious/malformed syscall arguments

AI might not understand RISC-V privilege levels deeply, so extra review is critical.

### Future (v2.0): AI Integration

Ironically, when we integrate AI acceleration into ThunderOS:
- Use AI tools to understand ML hardware interfaces
- Research AI framework integration with AI assistance
- But **deeply understand** the actual hardware and algorithms
- ThunderOS's AI features must be built on solid understanding, not just AI-generated code

## Questions?

If you're unsure whether your use of AI is appropriate:

1. **Ask yourself**: "Do I understand this code well enough to maintain it?"
2. **Ask yourself**: "Could I explain this to another developer?"
3. **Ask yourself**: "Would I trust this code in production?"

If the answer to any is "no", you need to do more research and understanding before committing.

## Conclusion

AI is a **powerful ally** in building ThunderOS, but **you** are the developer. AI accelerates your work, but doesn't replace your judgment, understanding, or responsibility.

**Use AI confidently. Understand deeply. Ship responsibly.** 🚀

---

**Last Updated**: November 3, 2025  
**Status**: Active Policy  
**Applies to**: All ThunderOS development (v0.1.0 onwards)

For questions or suggestions about this policy, open an issue or discussion on the ThunderOS repository.
