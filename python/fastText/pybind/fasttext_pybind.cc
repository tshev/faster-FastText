/**
 * Copyright (c) 2017-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <args.h>
#include <densematrix.h>
#include <fasttext.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <real.h>
#include <vector.h>
#include <cmath>
#include <iterator>
#include <numeric>
#include <sstream>
#include <stdexcept>

using namespace pybind11::literals;
namespace py = pybind11;

py::str castToPythonString(const std::string& s, const char* onUnicodeError) {
  PyObject* handle = PyUnicode_DecodeUTF8(s.data(), s.length(), onUnicodeError);
  if (!handle) {
    throw py::error_already_set();
  }

  // py::str's constructor from a PyObject assumes the string has been encoded
  // for python 2 and not encoded for python 3 :
  // https://github.com/pybind/pybind11/blob/ccbe68b084806dece5863437a7dc93de20bd9b15/include/pybind11/pytypes.h#L930
#if PY_MAJOR_VERSION < 3
  handle = PyUnicode_AsEncodedString(handle, "utf-8", onUnicodeError);
#endif

  return py::str(handle);
}

std::pair<std::vector<py::str>, std::vector<py::str>> getLineText(
    fasttext::FastText& m,
    const std::string& text,
    const char* onUnicodeError) {
  std::shared_ptr<const fasttext::Dictionary> d = m.getDictionary();
  std::stringstream ioss(text);
  std::string token;
  std::vector<py::str> words;
  std::vector<py::str> labels;
  while (d->readWord(ioss, token)) {
    uint32_t h = d->hash(token);
    int32_t wid = d->getId(token, h);
    fasttext::entry_type type = wid < 0 ? d->getType(token) : d->getType(wid);

    if (type == fasttext::entry_type::word) {
      words.push_back(castToPythonString(token, onUnicodeError));
      // Labels must not be OOV!
    } else if (type == fasttext::entry_type::label && wid >= 0) {
      labels.push_back(castToPythonString(token, onUnicodeError));
    }
    if (token == fasttext::Dictionary::EOS)
      break;
  }
  return {std::move(words), std::move(labels)};
}

PYBIND11_MODULE(fasttext_pybind, m) {
  py::class_<fasttext::Args>(m, "args")
      .def(py::init<>())
      .def_readwrite("input", &fasttext::Args::input)
      .def_readwrite("output", &fasttext::Args::output)
      .def_readwrite("lr", &fasttext::Args::lr)
      .def_readwrite("lrUpdateRate", &fasttext::Args::lrUpdateRate)
      .def_readwrite("dim", &fasttext::Args::dim)
      .def_readwrite("ws", &fasttext::Args::ws)
      .def_readwrite("epoch", &fasttext::Args::epoch)
      .def_readwrite("minCount", &fasttext::Args::minCount)
      .def_readwrite("minCountLabel", &fasttext::Args::minCountLabel)
      .def_readwrite("neg", &fasttext::Args::neg)
      .def_readwrite("wordNgrams", &fasttext::Args::wordNgrams)
      .def_readwrite("loss", &fasttext::Args::loss)
      .def_readwrite("model", &fasttext::Args::model)
      .def_readwrite("bucket", &fasttext::Args::bucket)
      .def_readwrite("minn", &fasttext::Args::minn)
      .def_readwrite("maxn", &fasttext::Args::maxn)
      .def_readwrite("thread", &fasttext::Args::thread)
      .def_readwrite("t", &fasttext::Args::t)
      .def_readwrite("label", &fasttext::Args::label)
      .def_readwrite("verbose", &fasttext::Args::verbose)
      .def_readwrite("pretrainedVectors", &fasttext::Args::pretrainedVectors)
      .def_readwrite("saveOutput", &fasttext::Args::saveOutput)

      .def_readwrite("qout", &fasttext::Args::qout)
      .def_readwrite("retrain", &fasttext::Args::retrain)
      .def_readwrite("qnorm", &fasttext::Args::qnorm)
      .def_readwrite("cutoff", &fasttext::Args::cutoff)
      .def_readwrite("dsub", &fasttext::Args::dsub);

  py::enum_<fasttext::model_name>(m, "model_name")
      .value("cbow", fasttext::model_name::cbow)
      .value("skipgram", fasttext::model_name::sg)
      .value("supervised", fasttext::model_name::sup)
      .export_values();

  py::enum_<fasttext::loss_name>(m, "loss_name")
      .value("hs", fasttext::loss_name::hs)
      .value("ns", fasttext::loss_name::ns)
      .value("softmax", fasttext::loss_name::softmax)
      .value("ova", fasttext::loss_name::ova)
      .export_values();

  m.def(
      "train",
      [](fasttext::FastText& ft, fasttext::Args& a) { ft.train(a); },
      py::call_guard<py::gil_scoped_release>());

  py::class_<fasttext::Vector>(m, "Vector", py::buffer_protocol())
      .def(py::init<ssize_t>())
      .def_buffer([](fasttext::Vector& m) -> py::buffer_info {
        return py::buffer_info(
            m.data(),
            sizeof(fasttext::real),
            py::format_descriptor<fasttext::real>::format(),
            1,
            {m.size()},
            {sizeof(fasttext::real)});
      });

  py::class_<fasttext::DenseMatrix>(
      m, "DenseMatrix", py::buffer_protocol(), py::module_local())
      .def(py::init<>())
      .def(py::init<ssize_t, ssize_t>())
      .def_buffer([](fasttext::DenseMatrix& m) -> py::buffer_info {
        return py::buffer_info(
            m.data(),
            sizeof(fasttext::real),
            py::format_descriptor<fasttext::real>::format(),
            2,
            {m.size(0), m.size(1)},
            {sizeof(fasttext::real) * m.size(1),
             sizeof(fasttext::real) * (int64_t)1});
      });

  py::class_<fasttext::FastText>(m, "fasttext")
      .def(py::init<>())
      .def("getArgs", &fasttext::FastText::getArgs)
      .def(
          "getInputMatrix",
          [](fasttext::FastText& m) {
            std::shared_ptr<const fasttext::DenseMatrix> mm =
                m.getInputMatrix();
            return *mm.get();
          })
      .def(
          "getOutputMatrix",
          [](fasttext::FastText& m) {
            std::shared_ptr<const fasttext::DenseMatrix> mm =
                m.getOutputMatrix();
            return *mm.get();
          })
      .def(
          "loadModel",
          [](fasttext::FastText& m, const std::string& s) { m.loadModel(s); })
      .def(
          "saveModel",
          [](fasttext::FastText& m, const std::string& s) { m.saveModel(s); })
      .def(
          "test",
          [](fasttext::FastText& m, const std::string& filename, int32_t k) {
            std::ifstream ifs(filename);
            if (!ifs.is_open()) {
              throw std::invalid_argument("Test file cannot be opened!");
            }
            fasttext::Meter meter;
            m.test(ifs, k, 0.0, meter);
            ifs.close();
            return std::tuple<int64_t, double, double>(
                meter.nexamples(), meter.precision(), meter.recall());
          })
      .def(
          "getSentenceVector",
          [](fasttext::FastText& m,
             fasttext::Vector& v,
             const std::string& text) {
            std::stringstream ioss(text);
            m.getSentenceVector(ioss, v);
          })
      .def(
          "tokenize",
          [](fasttext::FastText& m, const std::string& text) {
            std::vector<std::string> text_split;
            std::shared_ptr<const fasttext::Dictionary> d = m.getDictionary();
            std::stringstream ioss(text);
            std::string token;
            while (!ioss.eof()) {
              while (d->readWord(ioss, token)) {
                text_split.push_back(token);
              }
            }
            return text_split;
          })
      .def("getLine", &getLineText)
      .def(
          "multilineGetLine",
          [](fasttext::FastText& m,
             const std::vector<std::string>& lines,
             const char* onUnicodeError) {
            std::shared_ptr<const fasttext::Dictionary> d = m.getDictionary();

            std::vector<std::vector<py::str>> all_words;
            all_words.reserve(lines.size());
            std::vector<std::vector<py::str>> all_labels;
            all_labels.reserve(lines.size());

            for (const auto& text : lines) {
              auto pair = getLineText(m, text, onUnicodeError);
              all_words.push_back(std::move(pair.first));
              all_labels.push_back(std::move(pair.second));
            }
            return std::pair<
                std::vector<std::vector<py::str>>,
                std::vector<std::vector<py::str>>>(std::move(all_words), std::move(all_labels));
          })
      .def(
          "getVocab",
          [](fasttext::FastText& m, const char* onUnicodeError) {
            py::str s;
            std::shared_ptr<const fasttext::Dictionary> d = m.getDictionary();
            std::vector<int64_t> vocab_freq = d->getCounts(fasttext::entry_type::word);
            std::vector<py::str> vocab_list;
            vocab_list.reserve(vocab_freq.size());
            for (int32_t i = 0; i < vocab_freq.size(); i++) {
              vocab_list.push_back(castToPythonString(d->getWord(i), onUnicodeError));
            }
            return std::pair<std::vector<py::str>, std::vector<int64_t>>(
                std::move(vocab_list), std::move(vocab_freq));
          })
      .def(
          "getLabels",
          [](fasttext::FastText& m, const char* onUnicodeError) {
            std::shared_ptr<const fasttext::Dictionary> d = m.getDictionary();
            std::vector<int64_t> labels_freq = d->getCounts(fasttext::entry_type::label);
            std::vector<py::str> labels_list;
            labels_list.reserve(labels_freq.size());

            for (int32_t i = 0; i < labels_freq.size(); i++) {
              labels_list.push_back(
                  castToPythonString(d->getLabel(i), onUnicodeError));
            }
            return std::pair<std::vector<py::str>, std::vector<int64_t>>(
                std::move(labels_list), std::move(labels_freq));
          })
      .def(
          "quantize",
          [](fasttext::FastText& m,
             const std::string& input,
             bool qout,
             int32_t cutoff,
             bool retrain,
             int epoch,
             double lr,
             int thread,
             int verbose,
             int32_t dsub,
             bool qnorm) {
            fasttext::Args qa = fasttext::Args();
            qa.input = input;
            qa.qout = qout;
            qa.cutoff = cutoff;
            qa.retrain = retrain;
            qa.epoch = epoch;
            qa.lr = lr;
            qa.thread = thread;
            qa.verbose = verbose;
            qa.dsub = dsub;
            qa.qnorm = qnorm;
            m.quantize(qa);
          })
      .def(
          "predict",
          // NOTE: text needs to end in a newline
          // to exactly mimic the behavior of the cli
          [](fasttext::FastText& m,
             const std::string& text,
             int32_t k,
             fasttext::real threshold,
             const char* onUnicodeError) {
            std::stringstream ioss(text);
            std::vector<std::pair<fasttext::real, std::string>> predictions;
            m.predictLine(ioss, predictions, k, threshold);

            std::vector<std::pair<fasttext::real, py::str>> transformedPredictions;
            transformedPredictions.reserve(predictions.size());

            for (const auto& prediction : predictions) {
              transformedPredictions.emplace_back(
                  prediction.first,
                  castToPythonString(prediction.second, onUnicodeError)
              );
            }

            return transformedPredictions;
          })
      .def(
          "predictAll",
          // NOTE: text needs to end in a newline
          // to exactly mimic the behavior of the cli
          [](fasttext::FastText& m, const std::string& text) {
            std::stringstream ioss(text);
            std::vector<std::pair<fasttext::real, std::string>> predictions;

            m.predictLine(ioss, predictions);
            std::sort(std::begin(predictions), std::end(predictions), [](const auto& x, const auto &y) {
              return x.second < y.second;
            });

            fasttext::real sum = std::accumulate(std::begin(predictions), std::end(predictions), fasttext::real{0.0}, [](fasttext::real r, const auto& x) {
                return r + x.first;
            });

            std::vector<fasttext::real> transformedPredictions;
            transformedPredictions.reserve(predictions.size());

            if (sum == fasttext::real(0.0)) {
                std::transform(std::begin(predictions), std::end(predictions), std::back_inserter(transformedPredictions), [](const auto& x) {
                    return x.first;
                });
            } else {
                std::transform(std::begin(predictions), std::end(predictions), std::back_inserter(transformedPredictions), [sum](const auto& x) {
                    return x.first / sum;
                });
            }
            return transformedPredictions;
          })
      .def(
          "multilinePredict",
          // NOTE: text needs to end in a newline
          // to exactly mimic the behavior of the cli
          [](fasttext::FastText& m,
             const std::vector<std::string>& lines,
             int32_t k,
             fasttext::real threshold,
             const char* onUnicodeError) {
            std::vector<std::vector<std::pair<fasttext::real, py::str>>>
                allPredictions;
            allPredictions.reserve(lines.size());
            std::vector<std::pair<fasttext::real, std::string>> predictions;

            for (const std::string& text : lines) {
              std::stringstream ioss(text); /// stringstream is slow
              m.predictLine(ioss, predictions, k, threshold);
              std::vector<std::pair<fasttext::real, py::str>> transformedPredictions;
              transformedPredictions.reserve(predictions.size());
              for (const auto& prediction : predictions) {
                transformedPredictions.emplace_back(
                    prediction.first,
                    castToPythonString(prediction.second, onUnicodeError)
                );
              }
              allPredictions.push_back(std::move(transformedPredictions));
            }
            return allPredictions;
          })
      .def(
          "multilinePredictAll",
          // NOTE: text needs to end in a newline
          // to exactly mimic the behavior of the cli
          [](fasttext::FastText& m, const std::vector<std::string>& lines) {
            std::vector<std::vector<fasttext::real>> allPredictions;

            allPredictions.reserve(lines.size());
            std::vector<std::pair<fasttext::real, std::string>> predictions;
            for (const std::string& text : lines) {
              std::stringstream ioss(text); /// stringstream is slow
              m.predictLine(ioss, predictions);
              std::sort(std::begin(predictions), std::end(predictions), [](const auto& x, const auto &y) {
                return x.second < y.second;
              });
              std::vector<fasttext::real> transformedPredictions;
              transformedPredictions.reserve(predictions.size());
              std::transform(std::begin(predictions), std::end(predictions), std::back_inserter(transformedPredictions), [](const auto& x) {
                return x.first;
              });
              allPredictions.push_back(std::move(transformedPredictions));
            }
            return allPredictions;
          })

      .def(
          "testLabel",
          [](fasttext::FastText& m,
             const std::string& filename,
             int32_t k,
             fasttext::real threshold) {
            std::ifstream ifs(filename);
            if (!ifs.is_open()) {
              throw std::invalid_argument("Test file cannot be opened!");
            }
            fasttext::Meter meter;
            m.test(ifs, k, threshold, meter);
            std::shared_ptr<const fasttext::Dictionary> d = m.getDictionary();
            std::unordered_map<std::string, py::dict> returnedValue(d->nlabels());
            for (int32_t i = 0; i < d->nlabels(); i++) {
              returnedValue[d->getLabel(i)] = py::dict(
                  "precision"_a = meter.precision(i),
                  "recall"_a = meter.recall(i),
                  "f1score"_a = meter.f1Score(i));
            }

            return returnedValue;
          })
      .def(
          "getWordId",
          [](fasttext::FastText& m, const std::string& word) {
            return m.getWordId(word);
          })
      .def(
          "getSubwordId",
          [](fasttext::FastText& m, const std::string& word) {
            return m.getSubwordId(word);
          })
      .def(
          "getInputVector",
          [](fasttext::FastText& m, fasttext::Vector& vec, int32_t ind) {
            m.getInputVector(vec, ind);
          })
      .def(
          "getWordVector",
          [](fasttext::FastText& m,
             fasttext::Vector& vec,
             const std::string& word) { m.getWordVector(vec, word); })
      .def(
          "getSubwords",
          [](fasttext::FastText& m,
             const std::string& word,
             const char* onUnicodeError) {
            std::vector<std::string> subwords;
            std::vector<int32_t> ngrams;
            std::shared_ptr<const fasttext::Dictionary> d = m.getDictionary();
            d->getSubwords(word, ngrams, subwords);

            std::vector<py::str> transformedSubwords;
            transformedSubwords.reserve(subwords.size());

            for (const auto& subword : subwords) {
              transformedSubwords.push_back(castToPythonString(subword, onUnicodeError));
            }

            return std::pair<std::vector<py::str>, std::vector<int32_t>>(
                std::move(transformedSubwords), std::move(ngrams));
          })
      .def("isQuant", [](fasttext::FastText& m) { return m.isQuant(); });
}
