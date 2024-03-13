# How to Contribute

We welcome community contributions to `iamf-tools`. Thank you for your time! By
contributing to the project, you agree to the license, patent and copyright
terms in the AOM License and Patent License and to the release of your
contribution under these terms. See [LICENSE](LICENSE) and [PATENTS](PATENTS)
for details.

## Contributor agreement

You will be required to execute the appropriate
[contributor agreement](http://aomedia.org/license/) to ensure that the AOMedia
Project has the right to distribute your changes.

## Coding style

We are using the Google C++ Coding Style defined by the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

The coding style used by this project is enforced with clang-format using the
following configuration file in the root of the repository.
[.clang-format](.clang-format)

```
    # Apply clang-format to modified .cc, .h files
    $ clang-format -i --style=file \
      $(git diff --name-only --diff-filter=ACMR '*.cc' '*.h')
```
