# Password

To configure a password first duplicate the `config.example.h` file and remove the word `example` from the new file.

E.g. `config.example.h` -> `config.h`

Open up the newly created file and edit the constant variable `PASSWORD`. This is what the user must enter on the keypad before the gates will open.

# Submit

For a user to submit the pass phrase they have entered they must type a specific key. The default key that is defined in `config.example.h` is `#` but it can be any key you wish.

You can configure the key the same way you would for the password mentioned above.

NOTE: The `SUBMIT_KEY` value must be defined as a `char` that's why it has single quotes around it.
