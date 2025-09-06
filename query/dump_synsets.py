import wn

synsets = wn.synsets(pos='n')

counter = 0
with open('synsets.txt', 'w') as f:
    for synset in synsets:
        if len(synset.lemmas()) == 1:
            continue

        for word in synset.lemmas():
            f.write(word + ';')
        f.write('\n')

        counter += 1
        print(f"processed {counter} synsets out of {len(synsets)}")
