Code Quality Standards
=====================

All code contributions must adhere to the following standards:

**Mandatory Rules:**

1. **Descriptive Variable Names**
   
   Use full, descriptive names that clearly indicate the variable's purpose. Avoid abbreviations and single-letter names except for loop counters in trivial loops.
   
   .. code-block:: c
   
      // Good
      Token function_name;
      ASTNode *condition_expression;
      int array_element_count;
      
      // Bad
      Token fn;
      ASTNode *cond;
      int cnt;

2. **No Magic Numbers - Always Use Constants**
   
   Replace all numeric literals with named constants that explain their meaning.
   
   .. code-block:: c
   
      // Good
      #define MAX_PARAMETERS 128
      #define VHDL_BIT_WIDTH 32
      #define FIRST_CHILD_INDEX 0
      
      if (parameter_count < MAX_PARAMETERS) { ... }
      
      // Bad
      if (parameter_count < 128) { ... }

3. **Static Functions for Internal Helpers**
   
   All helper functions that are not part of the public API must be declared ``static`` to limit their scope to the current translation unit.
   
   .. code-block:: c
   
      // Good
      static void emit_mapped_signal_name(const char *variable_name, FILE *output_file);
      static int parse_array_dimensions(const char *var_name, char *array_name);
      
      // Bad (unless intended for public API)
      void emit_mapped_signal_name(const char *variable_name, FILE *output_file);

4. **Modular Functions - Divide Work into Focused Functions**
   
   Break down complex logic into small, focused helper functions. Each function should have a single, clear responsibility. Follow the same modular approach used in the parse source files.
   
   .. code-block:: c
   
      // Good - Clean dispatcher with focused helpers
      static void generate_node(ASTNode *node, FILE *output_file) {
          switch (node->type) {
              case NODE_WHILE_STATEMENT:
                  generate_while_loop(node, output_file);
                  break;
              case NODE_FOR_STATEMENT:
                  generate_for_loop(node, output_file);
                  break;
              // ...
          }
      }
      
      // Bad - Monolithic function handling everything inline

5. **Statement Bodies on Separate Lines from Conditions**
   
   Control flow statement bodies (if, while, for) must always be on separate lines from their conditions. This improves readability and makes debugging easier.
   
   .. code-block:: c
   
      // Good
      if (condition_expression != NULL)
      {
          process_condition(condition_expression);
      }
      
      while (index < array_size)
      {
          process_element(array[index]);
          index++;
      }
      
      // Bad
      if (condition_expression != NULL) { process_condition(condition_expression); }
      while (index < array_size) { process_element(array[index]); index++; }

6. **All Variables Must Be Initialized**
   
   Never leave variables uninitialized. Always provide explicit initial values, even if they will be immediately reassigned.
   
   .. code-block:: c
   
      // Good
      int child_index = 0;
      ASTNode *condition = NULL;
      char buffer[MAX_BUFFER_SIZE] = {0};
      const char *operator = NULL;
      
      // Bad
      int child_index;
      ASTNode *condition;
      char buffer[MAX_BUFFER_SIZE];

**Rationale:**

These standards ensure that code is:

* **Readable** - Anyone can understand the code's intent without extensive comments
* **Maintainable** - Changes can be made safely without unintended side effects
* **Debuggable** - Issues are easy to identify and fix
* **Consistent** - All code follows the same patterns and style
