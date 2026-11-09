## Hint 1

Start with the \`switch (operation)\` statement. Each enum value represents one arithmetic operation. Return a \`Result\` containing both the numeric value and an \`Error\` value.

## Hint 2

For the four normal operations, the result should use \`Error::None\`. For division, check whether \`right == 0.0\` before performing the division. Do not divide first and try to repair the result afterward.

## Hint 3

Return \`{left + right, Error::None}\`, \`{left - right, Error::None}\`, \`{left * right, Error::None}\`, or \`{left / right, Error::None}\` for successful operations. Return \`{0.0, Error::DivisionByZero}\` for a zero divisor, and return \`{0.0, Error::InvalidOperation}\` after the switch for an unrecognized enum value.
